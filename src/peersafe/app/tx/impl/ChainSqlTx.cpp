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

namespace ripple {

    ChainSqlTx::ChainSqlTx(ApplyContext& ctx)
        : Transactor(ctx)
    {

    }

	STEntry * ChainSqlTx::getTableEntry(ApplyView& view, const STTx& tx)
	{
		ripple::uint160  nameInDB;

		AccountID account;
		if (tx.isFieldPresent(sfOwner))
			account = tx.getAccountID(sfOwner);
		else if (tx.isFieldPresent(sfAccount))
			account = tx.getAccountID(sfAccount);
		else
			return NULL;
		auto const k = keylet::table(account);
		SLE::pointer pTableSle = view.peek(k);
		if (pTableSle == NULL)
			return NULL;

		auto &aTableEntries = pTableSle->peekFieldArray(sfTableEntries);
		
		if (!tx.isFieldPresent(sfTables))
			return NULL;
		auto const & sTxTables = tx.getFieldArray(sfTables);
		Blob vTxTableName = sTxTables[0].getFieldVL(sfTableName);
		uint160 uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);
		return getTableEntry(aTableEntries, vTxTableName);
	}

	STEntry * ChainSqlTx::getTableEntry(const STArray & aTables, Blob& vCheckName)
	{
		auto iter(aTables.end());
		iter = std::find_if(aTables.begin(), aTables.end(),
			[vCheckName](STObject const &item) {
			if (!item.isFieldPresent(sfTableName))  return false;
			if (!item.isFieldPresent(sfDeleted))    return false;

			return item.getFieldVL(sfTableName) == vCheckName && item.getFieldU8(sfDeleted) != 1;
		});

		if (iter == aTables.end())  return NULL;

		return (STEntry*)(&(*iter));
	}

    TER ChainSqlTx::preflight(PreflightContext const& ctx)
    {
        const STTx & tx = ctx.tx;
        Application& app = ctx.app;
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
        Application& app = ctx.app;
        auto j = app.journal("preflightChainSql");

        if (tx.isCrossChainUpload())
        {
            AccountID sourceID(tx.getAccountID(sfAccount));            
            auto key = keylet::table(sourceID);
            auto const tablesle = ctx.view.read(key);

            ripple::uint256 futureHash;
            if (tablesle && tablesle->isFieldPresent(sfFutureTxHash))
            {
                futureHash = tablesle->getFieldH256(sfFutureTxHash);
            }

            if (futureHash.isNonZero() && tx.getFieldH256(sfCurTxHash) != futureHash)
            {
                JLOG(j.trace()) << "currecnt hash is not equal to the expected hash.";
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
            auto accountId = tx.getAccountID(sfAccount);
            auto id = keylet::table(accountId);
            auto const tablesle1 = view.peek(id);
            tablesle1->setFieldH256(sfFutureTxHash, tx.getFieldH256(sfFutureTxHash));
            view.update(tablesle1);
        }
        return tesSUCCESS;
    }

	std::pair<TER, std::string> ChainSqlTx::dispose(TxStore& txStore, const STTx& tx)
	{
		auto pair =  txStore.Dispose(tx);
		if (pair.first)
			return std::make_pair(tesSUCCESS, pair.second);
		else
			return std::make_pair(tefTABLE_TXDISPOSEERROR, pair.second);
	}
}
