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
#include <peersafe/app/storage/TableStorage.h>

namespace ripple {
	ZXCAmount
		SqlStatement::calculateMaxSpend(STTx const& tx)
	{
		if (tx.isFieldPresent(sfSendMax))
		{
			auto const& sendMax = tx[sfSendMax];
			return sendMax.native() ? sendMax.zxc() : beast::zero;
		}
		/* If there's no sfSendMax in ZXC, and the sfAmount isn't
		-    in ZXC, then the transaction can not send ZXC. */
		auto const& saDstAmount = tx.getFieldAmount(sfAmount);
		return saDstAmount.native() ? saDstAmount.zxc() : beast::zero;
	}
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

		STArray const & aTableEntries(sleTable->getFieldArray(sfTableEntries));
		STEntry *pEntry = getTableEntry(aTableEntries, vTxTableName);
		if (pEntry)
		{
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
		uint160 uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);

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
		auto tmpret = preApply(ctx_.tx);
		if (!isTesSuccess(tmpret))
			return tmpret;

		//deal with operation-rule
		 tmpret = applyHandler(ctx_.view(), ctx_.tx, ctx_.app);
		if (!isTesSuccess(tmpret))
			return tmpret;

		return ChainSqlTx::doApply();
	}

	std::string SqlStatement::getOperationRule(ApplyView& view, const STTx& tx)
	{
		std::string rule;
		auto opType = tx.getFieldU16(sfOpType);
		STEntry *pEntry = getTableEntry(view, tx);
		if (pEntry != NULL)
			rule = pEntry->getOperationRule((TableOpType)opType);
		return rule;
	}

	TER SqlStatement::dealWithOperationRule(const STTx & tx,STEntry* pEntry)
	{
		auto optype = tx.getFieldU16(sfOpType);
		auto sOperationRule = pEntry->getOperationRule((TableOpType)optype);
		if (!sOperationRule.empty())
		{
			Json::Value jsonRule;
			if (!Json::Reader().parse(sOperationRule, jsonRule))
				return temBAD_OPERATIONRULE;
			std::string sRaw = strCopy(tx.getFieldVL(sfRaw));

			Json::Value jsonRaw;
			if (!Json::Reader().parse(sRaw, jsonRaw))
				return temBAD_RAW;
			if (optype == (int)R_INSERT)
			{
				//deal with insert condition 
				std::map<std::string, std::string> mapRule;
				std::string accountField;
				int insertLimit = -1;
				if (jsonRule.isMember(jss::Condition))
				{
					Json::Value& condition = jsonRule[jss::Condition];
					std::vector<std::string> members = condition.getMemberNames();
					// retrieve members in object
					for (size_t i = 0; i < members.size(); i++) {
						std::string field_name = members[i];
						mapRule[field_name] = condition[field_name].asString();
					}
				}

				if (jsonRule.isMember(jss::Count))
				{
					accountField = jsonRule[jss::Count][jss::AccountField].asString();
					insertLimit = jsonRule[jss::Count][jss::CountLimit].asInt();
				}

				for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
				{
					auto& v = jsonRaw[idx];
					std::vector<std::string> members = v.getMemberNames();
					// retrieve members in object
					for (size_t i = 0; i < members.size(); i++) {
						std::string field_name = members[i];

						if (mapRule.find(field_name) != mapRule.end())
						{
							std::string rule = mapRule[field_name];
							std::string value = v[field_name].asString();
							if (rule == "$account")
							{
								if (value != to_string(tx.getAccountID(sfAccount)))
									return tefTABLE_RULEDISSATISFIED;
							}
							else if (rule == "$tx_hash")
							{
								if (value != to_string(tx.getTransactionID()))
									return tefTABLE_RULEDISSATISFIED;
							}
							else
							{
								if (rule != value)
									return tefTABLE_RULEDISSATISFIED;
							}
						}
					}
					if (accountField != "")
					{
						bool bAccountRight = false;
						std::string sAccountID = to_string(tx.getAccountID(sfAccount));

						if (mapRule.find(accountField) != mapRule.end())
						{
							if (mapRule[accountField] == "$account" || mapRule[accountField] == sAccountID)
								bAccountRight = true;
						}
						else if (std::find(members.begin(), members.end(), accountField) != members.end())
						{
							if (v[accountField].asString() == sAccountID)
								bAccountRight = true;
						}
						if (!bAccountRight)
							return tefTABLE_RULEDISSATISFIED;
					}
					
				}

				// deal with insert count limit
				if (insertLimit > 0)
				{
					auto uNameInDB = pEntry->getFieldH160(sfNameInDB);
					auto id = keylet::insertlimit(tx.getAccountID(sfAccount));
					auto insertsle = ctx_.view().peek(id);
					if (!insertsle) 
					{
						insertsle = std::make_shared<SLE>(
							ltINSERTMAP, id.key);
						insertsle->setFieldVL(sfInsertCountMap, strCopy("{}"));
						ctx_.view().insert(insertsle);
					}
					std::string sCountMap = strCopy(insertsle->getFieldVL(sfInsertCountMap));
					Json::Value jsonMap;
					if (!Json::Reader().parse(sCountMap, jsonMap))
						return temUNKNOWN;
					int nCount = 0;
					auto sNameInDB = to_string(uNameInDB);
					if (jsonMap.isMember(sNameInDB)) 
					{
						nCount = jsonMap[sNameInDB].asInt();
					}
					if (nCount + jsonRaw.size() > insertLimit)
					{
						return temBAD_INSERTLIMIT;
					}
					jsonMap[sNameInDB] = nCount + jsonRaw.size();
					insertsle->setFieldVL(sfInsertCountMap, strCopy(jsonMap.toStyledString()));
					sCountMap = strCopy(insertsle->getFieldVL(sfInsertCountMap));
					ctx_.view().update(insertsle);
				}
			}
			else if (optype == (int)R_UPDATE)
			{
				std::vector<std::string> vecFields;
				Json::Value& fields = jsonRule[jss::Fields];
				for (Json::UInt idx = 0; idx < fields.size(); idx++)
				{
					vecFields.push_back(fields[idx].asString());
				}

				if (jsonRaw.size() <= 1)
					return temBAD_RAW; 

				if (vecFields.size() > 0) {
					// retrieve members in object				
					auto& v = jsonRaw[(Json::UInt)0];
					std::vector<std::string> members = v.getMemberNames();
					for (size_t i = 0; i < members.size(); i++)
					{
						std::string field_name = members[i];
						if (std::find(vecFields.begin(), vecFields.end(), field_name) == vecFields.end())
							return tefTABLE_RULEDISSATISFIED;
					}
				}
			}
		}
		//if (optype == (int)R_UPDATE)
		//{
		//	//cannot update 'AccountField' assigned in 'Insert'
		//	std::string sInsertRule = pEntry->getOperationRule(R_INSERT);
		//	if (!sInsertRule.empty()) {
		//		Json::Value insertRule;
		//		if (!Json::Reader().parse(sInsertRule, insertRule))
		//			return temBAD_OPERATIONRULE;
		//		if (insertRule.isMember(jss::Count))
		//		{
		//			std::string sRaw = strCopy(tx.getFieldVL(sfRaw));

		//			Json::Value jsonRaw;
		//			if (!Json::Reader().parse(sRaw, jsonRaw))
		//				return temBAD_RAW;

		//			std::string sAccountField = insertRule[jss::Count][jss::AccountField].asString();
		//			auto& v = jsonRaw[(Json::UInt)0];
		//			std::vector<std::string> members = v.getMemberNames();
		//			for (size_t i = 0; i < members.size(); i++)
		//				if (members[i] == sAccountField)
		//					return temBAD_RAW;
		//		}
		//	}
		//}

		return tesSUCCESS;
	}

	TER SqlStatement::preApply(const STTx & tx)
	{
		ApplyView& view = ctx_.view();
		Application& app = ctx_.app;

		auto sOperationRule = getOperationRule(view, tx);
		if (sOperationRule.empty())
			return tesSUCCESS;


		if (app.getTxStoreDBConn().GetDBConn() == nullptr ||
			app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
		{
			return tefDBNOTCONFIGURED;
		}

		ripple::TxStoreDBConn *pConn;
		ripple::TxStore *pStore;
		if (view.flags() & tapFromClient)
		{
			pConn = &app.getMasterTransaction().getClientTxStoreDBConn();
			pStore = &app.getMasterTransaction().getClientTxStore();
		}
		else
		{
			pConn = &app.getMasterTransaction().getConsensusTxStoreDBConn();
			pStore = &app.getMasterTransaction().getConsensusTxStore();
		}
		TxStoreTransaction stTran(pConn);
		TxStore& txStore = *pStore;

		auto tables = tx.getFieldArray(sfTables);
		uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);

		auto item = app.getTableStorage().GetItem(nameInDB);

		//canDispose is false if first_storage is true
		bool canDispose = true;
		if (item != NULL && item->isHaveTx(tx.getTransactionID()))
			canDispose = false;			

		if (canDispose)//not exist in storage list,so can dispose again, for case Duplicate entry 
		{
			auto result = dispose(txStore,tx);
			if (result.first == tesSUCCESS)
			{
				JLOG(app.journal("SqlStatement").trace()) << "Dispose success";
			}
			else
			{
				JLOG(app.journal("SqlStatement").trace()) << "Dispose error" << result.second;
				stTran.rollback();
				return result.first;
			}
		}
			
		stTran.rollback();
		return tesSUCCESS;
	}

	TER SqlStatement::adjustInsertCount(const STTx & tx, DatabaseCon* pConn)
	{
		STEntry *pEntry = getTableEntry(ctx_.view(), tx);
		std::string sOperationRule = "";
		if (pEntry != NULL) {
			sOperationRule = pEntry->getOperationRule(R_INSERT);
		}
		if (sOperationRule.empty())
			return tesSUCCESS;
		Json::Value jsonRule;
		Json::Reader().parse(sOperationRule, jsonRule);
		if (!jsonRule.isMember(jss::Count))
			return tesSUCCESS;
		std::string sAccountField = jsonRule[jss::Count][jss::AccountField].asString();
		int insertLimit = jsonRule[jss::Count][jss::CountLimit].asInt();
		// deal with insert count limit
		if (insertLimit > 0) {
			try {
				auto tables = tx.getFieldArray(sfTables);
				uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);

				std::string sql_str = boost::str(boost::format(
					R"(SELECT count(*) from t_%s WHERE %s = '%s';)")
					% to_string(nameInDB)
					% sAccountField
					% to_string(tx.getAccountID(sfAccount)));
				boost::optional<int> count;
				LockedSociSession sql_session = pConn->checkoutDb();
				soci::statement st = (sql_session->prepare << sql_str
					, soci::into(count));

				bool dbret = st.execute(true);

				if (dbret && count)
				{
					auto uNameInDB = pEntry->getFieldH160(sfNameInDB);
					auto id = keylet::insertlimit(tx.getAccountID(sfAccount));
					auto insertsle = ctx_.view().peek(id);
					if (insertsle)
					{
						std::string sCountMap = strCopy(insertsle->getFieldVL(sfInsertCountMap));
						Json::Value jsonMap;
						if (!Json::Reader().parse(sCountMap, jsonMap))
							return temUNKNOWN;
						auto sNameInDB = to_string(uNameInDB);
						if (jsonMap.isMember(sNameInDB))
						{
							jsonMap[sNameInDB] = *count;
						}
						insertsle->setFieldVL(sfInsertCountMap, strCopy(jsonMap.toStyledString()));
						ctx_.view().update(insertsle);
					}
				}
			}
			catch (std::exception &)
			{
				return temUNKNOWN;
			}
		}
		return tesSUCCESS;
	}

	std::pair<TER, std::string> SqlStatement::dispose(TxStore& txStore, const STTx& tx)
	{
		std::string sOperationRule;
		auto opType = tx.getFieldU16(sfOpType);
		STEntry *pEntry = getTableEntry(ctx_.view(), tx);
		if (pEntry != NULL) {
			sOperationRule = pEntry->getOperationRule((TableOpType)opType);
			//deal with operation-rule
			auto tmpret = dealWithOperationRule(tx, pEntry);
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
			//update insert sle if delete
			if (tx.getFieldU16(sfOpType) == R_DELETE)
			{
				TER ret = adjustInsertCount(tx, txStore.getDatabaseCon());
				if (!isTesSuccess(ret))
					return std::make_pair(ret, "Deal with delete rule error");;

			}
			return std::make_pair(tesSUCCESS, ret.second);
		}
		else
			return std::make_pair(tefTABLE_TXDISPOSEERROR, ret.second);
	}
}
// ripple
