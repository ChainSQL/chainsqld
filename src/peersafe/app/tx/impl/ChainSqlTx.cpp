//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#include <peersafe/app/tx/ChainSqlTx.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/app/storage/TableStorage.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <peersafe/rpc/TableUtils.h>

namespace ripple {

	ChainSqlTx::ChainSqlTx(ApplyContext& ctx)
		: Transactor(ctx)
	{

	}

	NotTEC ChainSqlTx::preflight(PreflightContext const& ctx)
	{
		const STTx & tx = ctx.tx;
		Schema& app = ctx.app;
		auto j = app.journal("preflightChainSql");

		if (tx.isCrossChainUpload() || (tx.isFieldPresent(sfOpType) && tx.getFieldU16(sfOpType) == T_REPORT))
		{
			if (!tx.isFieldPresent(sfOriginalAddress) ||
				!tx.isFieldPresent(sfTxnLgrSeq) ||
				!tx.isFieldPresent(sfCurTxHash) ||
				!tx.isFieldPresent(sfFutureTxHash)
				)
			{
				JLOG(j.trace()) <<
					"params are not invalid";
				return temINVALID;
			}
		}

		return tesSUCCESS;
	}
	TER ChainSqlTx::preclaim(PreclaimContext const& ctx)
	{
		const STTx & tx = ctx.tx;
		auto j = ctx.app.journal("preflightChainSql");

        if (ctx.tx.getTxnType() != ttSQLTRANSACTION && ctx.tx.getFieldU16(sfOpType) != T_DROP)
        {
            auto const sle = ctx.view.read(keylet::account(tx.getAccountID(sfAccount)));
            auto const balance = (*sle)[sfBalance].zxc();
            bool isContract = false;
            if (sle->isFieldPresent(sfContractCode)){
                isContract = true;
			}
            auto const reserve = ctx.view.fees().accountReserve(
                (*sle)[sfOwnerCount],isContract);
            if (balance < reserve + calculateFeePaid(tx))
                return tecINSUFFICIENT_RESERVE;
        }

		if (tx.isCrossChainUpload())
		{
			ripple::uint256 futureHash;
            std::shared_ptr<SLE const> tableSleExist = nullptr;
            std::tie(tableSleExist, std::ignore, std::ignore) =
                getTableEntry(ctx.view, tx);
            if (tableSleExist && tableSleExist->isFieldPresent(sfFutureTxHash))
			{
                futureHash = tableSleExist->getFieldH256(sfFutureTxHash);
			}

			if (futureHash.isNonZero() && tx.getFieldH256(sfCurTxHash) != futureHash)
			{
				JLOG(j.trace()) << "Current hash is not equal to the expected hash.";
				return temBAD_TRANSFERORDER;
			}
			if (futureHash.isZero() && tx.isFieldPresent(sfOpType) && tx.getFieldU16(sfOpType) == T_REPORT)
			{
				JLOG(j.trace()) << "T_REPORT can't be the first transfer operator.";
				return temBAD_TRANSFERORDER;
			}
		}
		return tesSUCCESS;
	}

	TER ChainSqlTx::doApply()
	{
		const STTx & tx = ctx_.tx;
		ApplyView & view = ctx_.view();

		if (tx.isCrossChainUpload())
		{
            std::shared_ptr<SLE> tableSleExist = nullptr;
			std::tie(tableSleExist, std::ignore, std::ignore) =  getTableEntryVar(view, tx);
            if (tableSleExist)
                tableSleExist->setFieldH256(
                    sfFutureTxHash, tx.getFieldH256(sfFutureTxHash));
            view.update(tableSleExist);
		}
		return tesSUCCESS;
	}

	std::pair<TER, std::string> ChainSqlTx::dispose(TxStore& txStore, const STTx& tx)
	{
		auto pair = txStore.Dispose(tx);
		if (pair.first)
			return std::make_pair(tesSUCCESS, pair.second);
		else
			return std::make_pair(tefTABLE_TXDISPOSEERROR, pair.second);
	}

	bool ChainSqlTx::canDispose(ApplyContext& ctx)
	{
		auto tables = ctx.tx.getFieldArray(sfTables);
		uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);

		auto item = ctx.app.getTableStorage().GetItem(nameInDB);

		//canDispose is false if first_storage is true
		bool canDispose = true;
		if (item != NULL && item->isHaveTx(ctx.tx.getTransactionID()))
			canDispose = false;

		return canDispose;
	}

	std::pair<TxStoreDBConn*, TxStore*> ChainSqlTx::getTransactionDBEnv(ApplyContext& ctx)
	{
		if (ctx.app.getTxStoreDBConn().GetDBConn() == nullptr ||
			ctx.app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
		{
			return std::make_pair<TxStoreDBConn*, TxStore*>(nullptr, nullptr);
		}

		ApplyView& view = ctx.view();
		ripple::TxStoreDBConn *pConn;
		ripple::TxStore *pStore;
		if (view.flags() & tapFromClient || view.flags() & tapByRelay)
		{
			pConn = &ctx.app.getMasterTransaction().getClientTxStoreDBConn();
			pStore = &ctx.app.getMasterTransaction().getClientTxStore();
		}
		else
		{
			pConn = &ctx.app.getMasterTransaction().getConsensusTxStoreDBConn();
			pStore = &ctx.app.getMasterTransaction().getConsensusTxStore();
		}
		return std::make_pair(pConn, pStore);
	}
}
