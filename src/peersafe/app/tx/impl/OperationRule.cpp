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
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/JsonFields.h>
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

std::string OperationRule::getOperationRule(ApplyView& view, const STTx& tx)
{
	std::string rule;
	auto opType = tx.getFieldU16(sfOpType);
	STEntry *pEntry = getTableEntry(view, tx);
	if (pEntry != NULL)
		rule = pEntry->getOperationRule((TableOpType)opType);
	return rule;
}

bool OperationRule::hasOperationRule(ApplyView& view, const STTx& tx)
{
	std::string rule = getOperationRule(view, tx);
	return !rule.empty();
}

bool OperationRule::checkRuleFields(std::vector<std::string>& vecFields, Json::Value condition)
{
	if (!condition.isObject())
		return false;
	std::vector<std::string> vecMembers = condition.getMemberNames();
	for (auto const& fieldName : vecMembers)
	{
		std::vector<std::string>::iterator iter;
		for (iter = vecFields.begin(); iter != vecFields.end(); iter++)
		{
			if (*iter == fieldName)
				break;
		}
		if (iter == vecFields.end())
		{
			if (fieldName.length() > 0 && fieldName[0] != '$')
				return false;
		}
		Json::Value& value = condition[fieldName];
		if (value.type() == Json::objectValue)
		{
			if (!checkRuleFields(vecFields, value))
				return false;
		}
		else if (value.type() == Json::arrayValue)
		{
			//eg:{'or':[{'id':1},{'name':'123'}]}
			for (auto &valueItem : value)
			{
				if (!valueItem.isObject())  
					return false;

				if (!checkRuleFields(vecFields, valueItem))
					return false;
			}
		}
	}
	return true;
}

TER OperationRule::dealWithTableListSetRule(ApplyContext& ctx, const STTx& tx)
{
	if (tx.getFieldU16(sfOpType) != T_CREATE)
		return tesSUCCESS;
	if (tx.isFieldPresent(sfOperationRule))
	{
		if (tx.isFieldPresent(sfToken))
			return temBAD_RULEANDTOKEN;

		auto j = ctx.app.journal("dealWithOperationRule");

		Json::Value jsonRule;
		auto sOperationRule = strCopy(tx.getFieldVL(sfOperationRule));
		if (Json::Reader().parse(sOperationRule, jsonRule))
		{
			std::string sRaw = strCopy(tx.getFieldVL(sfRaw));
			// will not dispose if raw is encrypted
			Json::Value jsonRaw;
			if (!Json::Reader().parse(sRaw, jsonRaw))
				return temBAD_RAW;

			std::vector<std::string> vecFields;
			for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
			{
				auto& v = jsonRaw[idx];
				if (v.isMember(jss::field))
				{
					vecFields.push_back(v[jss::field].asString());
				}
			}

			bool bContainCountLimit = false;
			std::string sAccountCondition = "";
			//make sure all fields in sfOperationRule corresponding to fields in sfRaw
			std::string accountField;
			if (jsonRule.isMember(jss::Insert))
			{
				if (jsonRule[jss::Insert].isMember(jss::Count))
				{
					const auto& jsonCount = jsonRule[jss::Insert][jss::Count];
					if (!jsonCount.isMember(jss::AccountField) || !jsonCount.isMember(jss::CountLimit))
						return temBAD_OPERATIONRULE;
					accountField = jsonCount[jss::AccountField].asString();
					if (std::find(vecFields.begin(), vecFields.end(), accountField) == vecFields.end())
						return temBAD_OPERATIONRULE;
					if (!jsonCount[jss::CountLimit].isInt() && !jsonCount[jss::CountLimit].isUInt())
						return temBAD_OPERATIONRULE;
					if (jsonCount[jss::CountLimit].asInt() <= 0)
						return temBAD_OPERATIONRULE;
					bContainCountLimit = true;
				}
				auto members = jsonRule[jss::Insert].getMemberNames();
				for (int i = 0; i < members.size(); i++)
				{
					if (members[i] != jss::Condition && members[i] != jss::Count)
						return temBAD_OPERATIONRULE;
				}
				if (jsonRule[jss::Insert].isMember(jss::Condition))
				{
					Json::Value& condition = jsonRule[jss::Insert][jss::Condition];
					if (!condition.isObject())
						return temBAD_OPERATIONRULE;
					std::vector<std::string> members = condition.getMemberNames();
					// retrieve members in object
					for (size_t i = 0; i < members.size(); i++) {
						if (std::find(vecFields.begin(), vecFields.end(), members[i]) == vecFields.end())
							return temBAD_OPERATIONRULE;
						//make sure account field right
						if (members[i] == accountField) {
							auto sAccount = condition[accountField].asString();
							if (sAccount != "$account" && ripple::parseBase58<AccountID>(sAccount) == boost::none)
								return temBAD_OPERATIONRULE;
							sAccountCondition = "\"" + accountField + "\" : \"" + sAccount + "\"";
						}
					}
				}
			}
			if (jsonRule.isMember(jss::Update))
			{
				auto members = jsonRule[jss::Update].getMemberNames();
				for (int i = 0; i < members.size(); i++)
				{
					if (members[i] != jss::Condition && members[i] != jss::Fields)
						return temBAD_OPERATIONRULE;
				}
				if (jsonRule[jss::Update].isMember(jss::Fields))
				{
					Json::Value jsonFields = jsonRule[jss::Update][jss::Fields];
					//RR-382
					if ( (0 == jsonFields.size()) && bContainCountLimit )
					{
						return temBAD_UPDATERULE;
					}
					//
					for (Json::UInt idx = 0; idx < jsonFields.size(); idx++)
					{
						if (std::find(vecFields.begin(), vecFields.end(), jsonFields[idx].asString()) == vecFields.end())
							return temBAD_OPERATIONRULE;
						if (jsonFields[idx].asString() == accountField)
							return temBAD_OPERATIONRULE;
					}
				}
				else if (bContainCountLimit) {
					return temBAD_UPDATERULE;
				}
				if (jsonRule[jss::Update].isMember(jss::Condition))
				{
					Json::Value& condition = jsonRule[jss::Update][jss::Condition];
					if (!condition.isObject())
						return temBAD_OPERATIONRULE;
					if(!checkRuleFields(vecFields,condition))
						return temBAD_OPERATIONRULE;
				}
			}
			else if (bContainCountLimit) {
				return temBAD_UPDATERULE;
			}

			if (jsonRule.isMember(jss::Delete))
			{
				auto members = jsonRule[jss::Delete].getMemberNames();
				if (members.size() != 1 || members[0] != jss::Condition)
					return temBAD_DELETERULE;

				if (!jsonRule[jss::Delete][jss::Condition].isObject())
					return temBAD_DELETERULE;
				if (!checkRuleFields(vecFields, jsonRule[jss::Delete][jss::Condition]))
					return temBAD_OPERATIONRULE;

				//if insert count is limited,then delete must define only the 'AccountField' account can delet
				if (bContainCountLimit)
				{
					if (!jsonRule[jss::Delete].isMember(jss::Condition))
					{
						return temBAD_DELETERULE;
					}
					//
					if (jsonRule[jss::Delete][jss::Condition].isMember("$or"))
						return temBAD_DELETERULE;
					else if (jsonRule[jss::Delete][jss::Condition].isMember("$and"))
					{
						bool bFound = false;
						Json::Value jsonFields = jsonRule[jss::Delete][jss::Condition]["$and"];
						if (!jsonFields.isArray())
							return temBAD_DELETERULE;
						for (Json::UInt idx = 0; idx < jsonFields.size(); idx++)
						{
							std::string sDelete = jsonFields[idx].toStyledString();
							if (sAccountCondition != "") {
								if (sDelete.find(sAccountCondition) != std::string::npos)
								{
									bFound = true;
									break;
								}
							}
							else
							{
								if (jsonFields[idx].isMember(accountField))
								{
									std::string strAccount = jsonFields[idx][accountField].asString();
									if (strAccount == "$account" || strAccount == to_string(tx.getAccountID(sfAccount)))
									{
										bFound = true;
										break;
									}
								}
							}
						}
						if (!bFound)
							return temBAD_DELETERULE;
					}
					else
					{
						if (sAccountCondition.empty())//RR-382
						{
							if (jsonRule[jss::Delete][jss::Condition].isMember(accountField))
							{
								std::string strAccount = jsonRule[jss::Delete][jss::Condition][accountField].asString();
								if (strAccount != "$account" && strAccount != to_string(tx.getAccountID(sfAccount)))
								{
									return temBAD_DELETERULE;
								}
							}
							else
								return temBAD_DELETERULE;
						}
						else
						{
							std::string sDelete = jsonRule[jss::Delete][jss::Condition].toStyledString();
							if (sDelete.find(sAccountCondition) == std::string::npos)
								return temBAD_DELETERULE;
						}
					}
				}
			}
			else if (bContainCountLimit)
			{
				return temBAD_DELETERULE;
			}

			if (jsonRule.isMember(jss::Get))
			{
				auto members = jsonRule[jss::Get].getMemberNames();
				if (members.size() != 1 || members[0] != jss::Condition)
					return temBAD_OPERATIONRULE;
				if (!jsonRule[jss::Get].isMember(jss::Condition))
					return temBAD_OPERATIONRULE;
				if (!checkRuleFields(vecFields, jsonRule[jss::Get][jss::Condition]))
					return temBAD_OPERATIONRULE;
			}
		}
		else
			return temBAD_OPERATIONRULE;
	}

	return tesSUCCESS;
}

TER OperationRule::dealWithSqlStatementRule(ApplyContext& ctx, const STTx& tx)
{
	STEntry* pEntry = getTableEntry(ctx.view(), tx);
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
				auto insertsle = ctx.view().peek(id);
				if (!insertsle)
				{
					if (jsonRaw.size() > insertLimit)
					{
						return temBAD_INSERTLIMIT;
					}
					return tesSUCCESS;
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

			if (jsonRaw.size() < 1)
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

	return tesSUCCESS;
}

TER OperationRule::adjustInsertCount(ApplyContext& ctx, const STTx& tx, DatabaseCon* pConn)
{
	STEntry* pEntry = getTableEntry(ctx.view(), tx);
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

			// deal with insert count limit
			if (insertLimit > 0)
			{
				auto uNameInDB = pEntry->getFieldH160(sfNameInDB);
				auto id = keylet::insertlimit(tx.getAccountID(sfAccount));
				auto insertsle = ctx.view().peek(id);
				if (!insertsle)
				{
					insertsle = std::make_shared<SLE>(
						ltINSERTMAP, id.key);
					insertsle->setFieldVL(sfInsertCountMap, strCopy("{}"));
					ctx.view().insert(insertsle);
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
				jsonMap[sNameInDB] = nCount + jsonRaw.size();
				JLOG(ctx.app.journal(__FUNCTION__).info()) << "R_INSERT:" << "tableRowCnt:" << nCount << "jsonRow:" << jsonRaw.size() << " sum:" << to_string(jsonMap[sNameInDB].asInt());

				insertsle->setFieldVL(sfInsertCountMap, strCopy(jsonMap.toStyledString()));
				sCountMap = strCopy(insertsle->getFieldVL(sfInsertCountMap));
				ctx.view().update(insertsle);
			}
		}
		else if (optype == (int)R_DELETE)
		{
			sOperationRule = pEntry->getOperationRule(R_INSERT);
			if (sOperationRule.empty())
				return tesSUCCESS;
			Json::Value jsonRule;
			Json::Reader().parse(sOperationRule, jsonRule);
			if (!jsonRule.isMember(jss::Count))
				return tesSUCCESS;
			std::string sAccountField = jsonRule[jss::Count][jss::AccountField].asString();
			int insertLimit = jsonRule[jss::Count][jss::CountLimit].asInt();
			// deal with insert count limit
			if (insertLimit > 0)
			{
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
						auto insertsle = ctx.view().peek(id);
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
								JLOG(ctx.app.journal(__FUNCTION__).trace()) << "R_DELETE:" << "tableRowCnt:" << *count;
							}
							insertsle->setFieldVL(sfInsertCountMap, strCopy(jsonMap.toStyledString()));
							ctx.view().update(insertsle);
						}
					}
				}
				catch (std::exception &)
				{
					return temUNKNOWN;
				}
			}
		}
	}

	return tesSUCCESS;
}

}