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

#include <BeastConfig.h>
#include <ripple/ledger/View.h>
#include <ripple/app/paths/RippleCalc.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/digest.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/tx/SqlStatement.h>
#include <peersafe/app/tx/OperationRule.h>
#include <peersafe/rpc/TableUtils.h>

namespace ripple {
	TER
		SqlStatement::preflightHandler(const STTx & tx, Application& app)
	{
		auto j = app.journal("preflightHandler");
		//check myown special fields
		if (!tx.isFieldPresent(sfOwner))
		{
			JLOG(j.trace()) << "Malformed transaction: " <<
				"Invalid owner";
			return temBAD_OWNER;
		}
		else
		{
			auto const &owner = tx.getAccountID(sfOwner);
			if (owner.isZero())
			{
				JLOG(j.trace()) << "Malformed transaction: " <<
					"Invalid owner";
				return temBAD_OWNER;
			}
		}

		if (!tx.isFieldPresent(sfOpType))
		{
			JLOG(j.trace()) << "Malformed transaction: " <<
				"Invalid opType";
			return temBAD_OPTYPE;
		}

		if (!tx.isFieldPresent(sfTables))
		{
			JLOG(j.trace()) << "Malformed transaction: " <<
				"Invalid table tables";
			return temBAD_TABLES;
		}
		else
		{
			if (tx.getFieldArray(sfTables).size() != 1)
			{
				JLOG(j.trace()) << "Malformed transaction: " <<
					"Invalid table size";
				return temBAD_TABLES;
			}
		}

		if (!tx.isFieldPresent(sfRaw))
		{
			JLOG(j.trace()) << "Malformed transaction: " <<
				"Invalid table flags";
			return temBAD_RAW;
		}
		else
		{
			Blob const raw = tx.getFieldVL(sfRaw);
			if (raw.empty())
			{
				JLOG(j.trace()) << "Malformed transaction: " <<
					"Invalid table flags";
				return temBAD_RAW;
			}
		}
		return tesSUCCESS;
	}

	TER
		SqlStatement::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;

		auto tmpret = preflightHandler(ctx.tx, ctx.app);
		if (!isTesSuccess(tmpret))
			return tmpret;

		tmpret = ChainSqlTx::preflight(ctx);
		if (!isTesSuccess(tmpret))
			return tmpret;

		return preflight2(ctx);
	}

	TER
		SqlStatement::preclaimHandler(ReadView const& view, const STTx & tx, Application& app)
	{
		auto j = app.journal("preclaimHandler");
		AccountID const uOwnerID(tx[sfOwner]);

		//whether the owner node is existed
		auto const kOwner = keylet::account(uOwnerID);
		auto const sleOwner = view.read(kOwner);
		if (!sleOwner)
		{
			JLOG(j.trace()) <<
				"Delay transaction: Destination account does not exist.";
			return tecNO_DST;
		}


		//whether the table node is existed
		auto const kTable = keylet::table(uOwnerID);
		auto const sleTable = view.read(kTable);
		if (!sleTable)
		{
			JLOG(j.trace()) <<
				"Delay transaction: Destination table node not exist.";
			return tecNO_TARGET;
		}

		auto optype = tx.getFieldU16(sfOpType);
		if ((optype < T_CREATE) || (optype > R_DELETE)) //check op range
			return temBAD_OPTYPE;

		auto const & sTxTables = tx.getFieldArray(sfTables);
		Blob vTxTableName = sTxTables[0].getFieldVL(sfTableName);
        uint160 uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);

		STArray const & aTableEntries(sleTable->getFieldArray(sfTableEntries));
		STEntry *pEntry = getTableEntry(aTableEntries, vTxTableName);        
		if (pEntry)
		{
            //checkDBName
            if (uTxDBName != pEntry->getFieldH160(sfNameInDB))
            {
                return tefBAD_DBNAME;
            }
			// strict mode
			if (tx.isFieldPresent(sfTxCheckHash))
			{
				uint256 hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))), pEntry->getFieldH256(sfTxCheckHash));
				if (hashNew != tx.getFieldH256(sfTxCheckHash))
				{
					return temBAD_BASETX;
				}
			}
			//check authority
			AccountID sourceID(tx.getAccountID(sfAccount));
			auto tableFalgs = getFlagFromOptype((TableOpType)tx.getFieldU16(sfOpType));
			if (!pEntry->hasAuthority(sourceID, tableFalgs))
			{
				JLOG(j.trace()) <<
					"Invalid table flags: Destination table does not authorith this account.";
				return tefBAD_AUTH_NO;
			}
		}
		else
		{
			JLOG(j.trace()) <<
				"Table is not exist for this account";
			return tefTABLE_NOTEXIST;
		}

		return tesSUCCESS;
	}

	TER
		SqlStatement::preclaim(PreclaimContext const& ctx)
	{
		auto tmpret = preclaimHandler(ctx.view, ctx.tx, ctx.app);
		if (!isTesSuccess(tmpret))
			return tmpret;

		return ChainSqlTx::preclaim(ctx);
	}

	TER
		SqlStatement::applyHandler(ApplyView& view, const STTx & tx, Application& app)
	{
		ripple::uint160  nameInDB;

		//set the tx link relationship to the node
		auto const curTxOwnID = tx.getAccountID(sfOwner);
		auto const k = keylet::table(curTxOwnID);
		SLE::pointer pTableSle = view.peek(k);

		auto &aTableEntries = pTableSle->peekFieldArray(sfTableEntries);

		auto const & sTxTables = tx.getFieldArray(sfTables);
		Blob vTxTableName = sTxTables[0].getFieldVL(sfTableName);		

		STEntry *pEntry = getTableEntry(aTableEntries, vTxTableName);
		if (pEntry)
		{
			uint256 hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))), pEntry->getFieldH256(sfTxCheckHash));
			if (tx.isFieldPresent(sfTxCheckHash))
			{
				assert(hashNew == tx.getFieldH256(sfTxCheckHash));
			}

			pEntry->setFieldH256(sfTxCheckHash, hashNew);

			if (pEntry->getFieldU32(sfTxnLgrSeq) != view.info().seq || pEntry->getFieldH256(sfTxnLedgerHash) != view.info().hash)
			{
				pEntry->setFieldU32(sfPreviousTxnLgrSeq, pEntry->getFieldU32(sfTxnLgrSeq));
				pEntry->setFieldH256(sfPrevTxnLedgerHash, pEntry->getFieldH256(sfTxnLedgerHash));

				pEntry->setFieldU32(sfTxnLgrSeq, view.info().seq);
				pEntry->setFieldH256(sfTxnLedgerHash, view.info().hash);
			}
		}

		view.update(pTableSle);
		return tesSUCCESS;
	}

	TER
		SqlStatement::doApply()
	{
		// dispose 
		auto tmpret = preApplyForOperationRule(ctx_.tx);
		if (!isTesSuccess(tmpret))
			return tmpret;

		//deal with operation-rule
		 tmpret = applyHandler(ctx_.view(), ctx_.tx, ctx_.app);
		if (!isTesSuccess(tmpret))
			return tmpret;

		return ChainSqlTx::doApply();
	}

	TER SqlStatement::preApplyForOperationRule(const STTx & tx)
	{
		ApplyView& view = ctx_.view();
		Application& app = ctx_.app;

		if(!OperationRule::hasOperationRule(view,tx))
			return tesSUCCESS;
		//
		if (ctx_.app.getTxStoreDBConn().GetDBConn() == nullptr ||
			ctx_.app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
		{
			return tefDBNOTCONFIGURED;
		}
		auto envPair = getTransactionDBEnv(ctx_);
		if (envPair.first == nullptr && envPair.second == nullptr)
		{
			return tefDBNOTCONFIGURED;
		}
		TxStoreTransaction stTran(envPair.first);
		TxStore& txStore = *envPair.second;
		//
		if (!canDispose(ctx_))
		{
			TER ret2 = OperationRule::adjustInsertCount(ctx_, tx, txStore.getDatabaseCon());//RR-628
			if (!isTesSuccess(ret2))
			{
				JLOG(app.journal("SqlStatement").trace()) << "Dispose error" << "Deal with count limit rule error";
				setExtraMsg("Dispose error: Deal with count limit rule error.");
				return ret2;
			}
			return tesSUCCESS;
		}

		if (app.getTxStoreDBConn().GetDBConn() == nullptr ||
			app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
		{
			return tefDBNOTCONFIGURED;
		}


		auto result = dispose(txStore, tx);
		if (result.first == tesSUCCESS)
		{
			TER ret2 = OperationRule::adjustInsertCount(ctx_, tx, txStore.getDatabaseCon());
			if (!isTesSuccess(ret2))
			{
				JLOG(app.journal("SqlStatement").trace()) << "Dispose error" << "Deal with count limit rule error";
				return ret2;
			}
			JLOG(app.journal("SqlStatement").trace()) << "Dispose success";
		}
		else
		{
			JLOG(app.journal("SqlStatement").trace()) << "Dispose error" << result.second;
			stTran.rollback();
			setExtraMsg(result.second);
			return result.first;
		}
			
		stTran.rollback();
		return tesSUCCESS;
	}


	std::pair<TER, std::string> SqlStatement::dispose(TxStore& txStore, const STTx& tx)
	{
		std::string sOperationRule = OperationRule::getOperationRule(ctx_.view(), tx);
		if (!sOperationRule.empty()) {
			//deal with operation-rule
			auto tmpret = OperationRule::dealWithSqlStatementRule(ctx_, tx);
			if (!isTesSuccess(tmpret))
				return std::make_pair(tmpret, "deal with operation-rule error");
		}

		//dispose
		std::pair<bool, std::string> ret;
		if (!sOperationRule.empty())
			ret = txStore.Dispose(tx, sOperationRule, true);
		else
			ret = txStore.Dispose(tx);
		if (ret.first)
		{
			return std::make_pair(tesSUCCESS, ret.second);
		}
		else
		{
			return std::make_pair(tefTABLE_TXDISPOSEERROR, ret.second);
		}			
	}
}
// ripple
