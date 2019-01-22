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
#include <ripple/json/json_reader.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/net/RPCErr.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/storage/TableStorage.h>
#include <peersafe/app/tx/TableListSet.h>
#include <peersafe/app/tx/SqlStatement.h>
#include <peersafe/app/tx/SqlTransaction.h>
#include <peersafe/app/tx/OperationRule.h>

namespace ripple {

	std::pair<TER, bool> SqlTransaction::transactionImpl_FstStorage(ApplyContext& ctx_, ripple::TxStore& txStore, TxID txID, beast::Journal journal, const std::vector<STTx> &txs)
	{
		auto tables = txs.at(0).getFieldArray(sfTables);
		uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);
		auto item = ctx_.app.getTableStorage().GetItem(nameInDB);
		if (item != NULL && item->isHaveTx(txID))
		{
			for (auto txTmp : txs)
			{
				TER ret2 = OperationRule::adjustInsertCount(ctx_, txTmp, txStore.getDatabaseCon());
				if (!isTesSuccess(ret2))
				{
					JLOG(journal.trace()) << "Dispose error" << "Deal with count limit rule error";
					return std::make_pair(ret2, true);
				}

			}
			return{ tesSUCCESS, true };
		}
		return{ tesSUCCESS, false };
	}

	std::pair<TER, std::string> SqlTransaction::transactionImpl(ApplyContext& ctx_, ripple::TxStoreDBConn &txStoreDBConn, ripple::TxStore& txStore, beast::Journal journal, const STTx &tx)
	{
		STTx tmpTx = tx;
		std::vector<STTx> txs = tx.getTxs(tmpTx);
		//
		TxID txID = tx.getTransactionID();
		auto ret = transactionImpl_FstStorage(ctx_, txStore, txID, journal, txs);
		if (ret.first != tesSUCCESS || ret.second)
			return std::make_pair(ret.first,"");

		/*
		 * drop table before execute the sql.
		 * do this first, because dropTable is not one step of Transaction.
		 * 
		 */
		for (auto txTmp : txs)
		{
			if (txTmp.getFieldU16(sfOpType) == T_CREATE)
			{
				auto &tables = txTmp.getFieldArray(sfTables);
				uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);
				txStore.DropTable(to_string(nameInDB));
			}
		}

		{
			std::vector<uint160> vecNameInDB;
			std::pair<TER, std::string> breakRet = { tesSUCCESS,"success" };
			TxStoreTransaction stTran(&txStoreDBConn);
			for (auto txTmp : txs)
			{
				bool canDispose = true;
				TableOpType opType = (TableOpType)txTmp.getFieldU16(sfOpType);
				//
				if (opType == T_CREATE)
				{
					auto &tables = txTmp.getFieldArray(sfTables);
					uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);
					vecNameInDB.push_back(nameInDB);
				}
				//OpType not need to dispose
				if (isNotNeedDisposeType(opType))
					canDispose = false;


				if (canDispose)//not exist in storage list,so can dispose again, for case Duplicate entry 
				{
					auto result = dispose(txStore, txTmp);
					if (result.first == tesSUCCESS)
					{
						TER ret2 = OperationRule::adjustInsertCount(ctx_, txTmp, txStore.getDatabaseCon());
						if (!isTesSuccess(ret2))
						{
							JLOG(journal.trace()) << "Dispose error" << "Deal with count limit rule error";
							breakRet = std::make_pair(ret2, "Dispose error: Deal with count limit rule error");
							break;
						}
						JLOG(journal.trace()) << "Dispose success";
					}
					else
					{
						JLOG(journal.trace()) << "Dispose error" << result.second;
						breakRet = result;
						break;
					}
				}
			}
			stTran.rollback();
			// drop table if created
			if (vecNameInDB.size() > 0)
			{
				for (auto nameInDB : vecNameInDB)
				{
					txStore.DropTable(to_string(nameInDB));
				}
			}
			if (breakRet.first != tesSUCCESS)
			{
				setExtraMsg(breakRet.second);
				return breakRet;
			}
		}
		return{ tesSUCCESS, "success" };
	}

	std::pair<TER, std::string> SqlTransaction::dispose(TxStore& txStore, const STTx& tx)
	{
		std::shared_ptr<ChainSqlTx> pTx;
		if (isSqlStatementOpType((TableOpType)tx.getFieldU16(sfOpType)))
			pTx = std::make_shared<SqlStatement>(ctx_);
		else if (isTableListSetOpType((TableOpType)tx.getFieldU16(sfOpType)))
			pTx = std::make_shared<TableListSet>(ctx_);
		if (pTx == nullptr)
			return std::make_pair(tefTABLE_TXDISPOSEERROR, "");
		try {
			return pTx->dispose(txStore, tx);
		}catch (std::exception const& e) {
			return std::make_pair(tefTABLE_TXDISPOSEERROR, e.what());
		}
		
	}

    ripple::TER
        SqlTransaction::handleEachTx(ApplyContext& ctx)
    {
        ripple::AccountID accountID = ctx.tx.getAccountID(sfAccount);
        Blob txs_blob = ctx.tx.getFieldVL(sfStatements);
        ripple::uint256 txId = ctx.tx.getTransactionID();
        std::string txs_str;

        txs_str.assign(txs_blob.begin(), txs_blob.end());
        Json::Value objs;
        Json::Reader().parse(txs_str, objs);
        ripple::TER result = tesSUCCESS;

        for (auto obj : objs)
        {
            try
            {
                //STTx tx(obj, accountID);
				auto tx_pair = STTx::parseSTTx(obj, accountID);
				auto tx = *tx_pair.first;
				tx.setParentTxID(ctx.tx.isSubTransaction() ? ctx.tx.getParentTxID() : ctx.tx.getTransactionID());
                if (obj["OpType"].asInt() != T_ASSERT) {
                    auto type = tx.getFieldU16(sfTransactionType);
                    if (type == ttTABLELISTSET)
                    {
                        result = TableListSet::preflightHandler(tx, ctx.app);
                        if (result == tesSUCCESS)
                        {
							if (ctx.tx.isCrossChainUpload()&&
								(obj["OpType"].asInt() == T_GRANT ||
								obj["OpType"].asInt() == T_ASSIGN ||
								obj["OpType"].asInt() == T_CANCELASSIGN) )
							{
								tx.setAccountID(sfOriginalAddress, ctx.tx.getAccountID(sfOriginalAddress));
								tx.setFieldU32(sfTxnLgrSeq, ctx.tx.getFieldU32(sfTxnLgrSeq));
								tx.setFieldH256(sfCurTxHash, ctx.tx.getFieldH256(sfCurTxHash));
								tx.setFieldH256(sfFutureTxHash, ctx.tx.getFieldH256(sfFutureTxHash));
							}
                            result = TableListSet::preclaimHandler(ctx.view(), tx, ctx.app);
                            if (result == tesSUCCESS) {
                                result = TableListSet::applyHandler(ctx.view(), tx, ctx.app);
                                if (result != tesSUCCESS)
                                {
                                    return std::move(result);
                                }
                            }
                            else
                                break;
                        }
                        else
                            break;
                    }
                    else if (type == ttSQLSTATEMENT)
                    {
						if (OperationRule::hasOperationRule(ctx.view(), tx) && ctx_.tx.getFieldU32(sfNeedVerify) != 1)
						{
							return temBAD_NEEDVERIFY_OPERRULE;
						}
                        result = SqlStatement::preflightHandler(tx, ctx.app);
                        if (result == tesSUCCESS) {
                            result = SqlStatement::preclaimHandler(ctx.view(), tx, ctx.app);
                            if (result == tesSUCCESS) {
                                result = SqlStatement::applyHandler(ctx.view(), tx, ctx.app);
                                if (result != tesSUCCESS)
                                {
                                    return std::move(result);
                                }
                            }
                            else
                                break;
                        }
                        else
                            break;
                    }
                }
            }
            catch (std::exception const& /*e*/)
            {
                return std::move(result);
            }
        }
        return std::move(result);
    }

    ZXCAmount
        SqlTransaction::calculateMaxSpend(STTx const& tx)
    {
        if (tx.isFieldPresent(sfSendMax))
        {
            auto const& sendMax = tx[sfSendMax];
            return sendMax.native() ? sendMax.zxc() : beast::zero;
        }
        /* If there's no sfSendMax in ZXC, and the sfAmount isn't
        in ZXC, then the transaction can not send ZXC. */
        auto const& saDstAmount = tx.getFieldAmount(sfAmount);
        return saDstAmount.native() ? saDstAmount.zxc() : beast::zero;
    }

    TER
        SqlTransaction::preflight(PreflightContext const& ctx)
    {
        auto  ret = preflight1(ctx);
        if (!isTesSuccess(ret))
            return ret;

        auto& tx = ctx.tx;
        auto& j = ctx.j;

        if (!tx.isFieldPresent(sfStatements))
        {
            JLOG(j.trace()) << "Malformed transaction: " <<
                "No Statement Field";
            return temBAD_STATEMENTS;
        }

        if (!tx.isFieldPresent(sfNeedVerify))
        {
            JLOG(j.trace()) << "Malformed transaction: " <<
                "No NeedVerify Field";
            return temBAD_NEEDVERIFY;
        }

        ret = ChainSqlTx::preflight(ctx);
        if (!isTesSuccess(ret))
            return ret;
        return preflight2(ctx);
    }

    TER
        SqlTransaction::preclaim(PreclaimContext const& ctx)  //just do some pre job
    {   
        return ChainSqlTx::preclaim(ctx);
    }

    TER
        SqlTransaction::doApply()
    {
        try {
			//preflight,preclaim,apply
			auto result = handleEachTx(ctx_);
			if (result != tesSUCCESS)
				return result;

			//database verify
			std::pair<TER, std::string> ret;
            if (ctx_.tx.getFieldU32(sfNeedVerify) == 1) {
				auto envPair = getTransactionDBEnv(ctx_);
				if (envPair.first == nullptr && envPair.second == nullptr)
				{
					return tefDBNOTCONFIGURED;
				}
				ret = transactionImpl(ctx_, *envPair.first, *envPair.second, ctx_.journal, ctx_.tx); //handle transaction,need DBTrans
				if (ret.first != tesSUCCESS)
					return ret.first;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(ctx_.journal.error()) << "doApply exception:" << e.what();
            return tefFAILURE;
        }


        return ChainSqlTx::doApply();
    }

}  // ripple
