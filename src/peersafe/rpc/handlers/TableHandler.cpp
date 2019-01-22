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
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/Role.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/app/main/Application.h>
#include <peersafe/app/storage/TableStorage.h> 
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/app/table/TableStatusDB.h>
#include <iostream> 
#include <fstream>

namespace ripple {

#define MAX_DIFF_TOLERANCE 3

void buildRaw(Json::Value& condition, std::string& rule);
int getDiff(RPC::Context& context, const std::vector<ripple::uint160>& vec);

//from rpc or http
Json::Value doRpcSubmit(RPC::Context& context)
{
	Json::Value& tx_json(context.params["tx_json"]);
	auto ret = context.app.getTableAssistant().prepare(context.params["secret"].asString(),context.params["public_key"].asString(), tx_json);
	if (ret.isMember("error") || ret.isMember("error_message"))
		return ret;

    ret = doSubmit(context);
    return ret;
}

Json::Value doCreateFromRaw(RPC::Context& context)
{ 
    using namespace std;
    Json::Value& tx_json(context.params);
    Json::Value& jsons(tx_json["params"]);
    Json::Value create_json;
    std::string secret;
    std::string tablename;
    std::string filename;
    for (auto json : jsons)
    {
        Json::Reader reader;
        Json::Value params_json;
        if (reader.parse(json.asString(), params_json))
        {
            create_json["Account"] = params_json["Account"];
            secret = params_json["Secret"].asString();
            tablename = params_json["TableName"].asString();
            filename = params_json["RawPath"].asString();
        }
    }

    Json::Value jvResult;
    ifstream myfile;
    myfile.open(filename, ios::in);
    if (!myfile)
    {    
        jvResult[jss::error_message] = "can not open file,please checkout path!";
        jvResult[jss::error] = "error";
        return jvResult;
    }

    char ch;
    string content;
    while (myfile.get(ch))
        content += ch;
    myfile.close();

    create_json["TransactionType"] = "TableListSet";
    create_json["Raw"] = content;
    create_json["OpType"] = T_CREATE;
    Json::Value tables_json;
    Json::Value table_json;
    Json::Value table;
    table["TableName"] = tablename;

    //AccountID accountID(*ripple::parseBase58<AccountID>(create_json["Account"].asString()));
    create_json["TableName"] = tablename;
    context.params["tx_json"] = create_json;
    auto ret = doGetDBName(context);

    if (ret["nameInDB"].asString().size()== 0)
    {
        jvResult[jss::error_message] = "can not getDBName,please checkout program is had sync!";
        jvResult[jss::error] = "error";
        return jvResult;
    }

    table["NameInDB"] = ret["nameInDB"];

    create_json.removeMember("TableName");
    table_json["Table"] = table;
    tables_json.append(table_json);
    create_json["Tables"] = tables_json;

    context.params["command"] = "t_create";
    context.params["secret"] = secret;
    context.params["tx_json"] = create_json;
    return doRpcSubmit(context);
}

Json::Value checkForSelect(RPC::Context&  context, uint160 nameInDB, std::vector<ripple::uint160> vecNameInDB)
{
	Json::Value ret(Json::objectValue);
	Json::Value& tx_json(context.params["tx_json"]);

	if (tx_json["Owner"].asString().empty())
	{
		ret[jss::error] = "field Owner is empty!";
		return ret;
	}
	auto ownerID = ripple::parseBase58<AccountID>(tx_json["Owner"].asString());
	if (ownerID == boost::none)
	{
		ret[jss::error] = "field Owner is wrong!";
		return ret;
	}

	if (tx_json["Account"].asString().empty())
	{
		ret[jss::error] = "field Account is empty!";
		return ret;
	}
	auto accountID = ripple::parseBase58<AccountID>(tx_json["Account"].asString());
	if (accountID == boost::none)
	{
		ret[jss::error] = "field Account is wrong!";
		return ret;
	}


	Json::Value &tables_json = tx_json["Tables"];
	if (!tables_json.isArray())
	{
		ret[jss::error] = "field Tables is not array!";
		return ret;
	}

	auto j = context.app.journal("RPCHandler");
	JLOG(j.debug())
		<< "get record from tables: " << tx_json.toStyledString();

	std::list<std::string> listTableName;
	for (Json::UInt idx = 0; idx < tables_json.size(); idx++) {
		Json::Value& e = tables_json[idx];
		if (!e.isObject())
		{
			ret[jss::error] = "field Tables is not object!";
			return ret;
		};

		Json::Value& v = e["Table"];
		if (!v.isObject())
		{
			ret[jss::error] = "field Table is not object!";
			return ret;
		}

		Json::Value tn = v["TableName"];
		if (!tn.isString())
		{
			ret[jss::error] = "field TableName is not string!";
			return ret;
		}

		auto nameInDBGot = context.ledgerMaster.getNameInDB(context.ledgerMaster.getValidLedgerIndex(), *ownerID, v["TableName"].asString());
		if (!nameInDBGot)
		{
			ret[jss::error] = "can't get TableName in DB ,please check field tablename!";
			return ret;
		}
		listTableName.push_back(v["TableName"].asString());
		// NameInDB is optional
		if (v.isMember("NameInDB"))
		{
			//NameInDB is filled
			std::string sNameInDB = v["NameInDB"].asString();
			if (sNameInDB.length() > 0)
			{
				nameInDB = ripple::from_hex_text<ripple::uint160>(sNameInDB);
				if (nameInDBGot == nameInDB)
				{
					v["TableName"] = sNameInDB;
				}
				else
				{
					ret[jss::error] = "please make sure the given 'NameInDB' in accordance with the 'TableName'";
					return ret;
				}
			}
			else
			{
				ret[jss::error] = "field NameInDB is empty!";
				return ret;
			}
		}
		else
		{
			//NameInDB is absent
			nameInDB = nameInDBGot;
			v["TableName"] = to_string(nameInDBGot);
		}
		vecNameInDB.push_back(nameInDB);
	}

	//check the authority
	auto retPair = context.ledgerMaster.isAuthorityValid(*accountID, *ownerID, listTableName, lsfSelect);
	if (!retPair.first)
	{
		ret[jss::error] = retPair.second;
		return ret;
	}

	std::string rule;
	auto ledger = context.ledgerMaster.getValidatedLedger();
	if (ledger)
	{
		auto id = keylet::table(*ownerID);
		auto const tablesle = ledger->read(id);

		//judge if account is activated
		auto key = keylet::account(*accountID);
		if (!ledger->exists(key))
		{
			ret[jss::error] = "account not valid";
			return ret;
		}

		if (tablesle)
		{
			auto aTableEntries = tablesle->getFieldArray(sfTableEntries);
			STEntry* pEntry = getTableEntry(aTableEntries, listTableName.front());
			if (pEntry)
				rule = pEntry->getOperationRule(R_GET);
		}
	}
	if (rule != "")
	{
		Json::Value conditions;
		Json::Value jsonRaw = tx_json[jss::Raw];
		if (jsonRaw.isString()) {
			std::string sRaw = jsonRaw.asString();
			Json::Reader().parse(sRaw, jsonRaw);
			if (!jsonRaw.isArray())
			{
				ret[jss::error] = "Raw not valid";
				return ret;
			}
		}
		for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
		{
			auto& v = jsonRaw[idx];
			if (idx == 0)
			{
				if (!v.isArray())
				{
					ret[jss::error] = "Raw has a wrong format ,first element must be an array";
					return ret;
				}
			}
			else
			{
				conditions.append(v);
			}
		}

		StringReplace(rule, "$account", tx_json["Account"].asString());
		if (conditions.isArray())
		{
			Json::Value newRaw;
			buildRaw(conditions, rule);
		}
		Json::Value finalRaw;
		if (jsonRaw.size() > 0)
			finalRaw.append(jsonRaw[(Json::UInt)0]);
		else
		{
			Json::Value arr(Json::arrayValue);
			finalRaw.append(arr);
		}

		for (Json::UInt idx = 0; idx < conditions.size(); idx++)
		{
			finalRaw.append(conditions[idx]);
		}
		tx_json["Raw"] = finalRaw;
	}

	if (tx_json["Raw"].isArray())
	{
		tx_json["Raw"] = tx_json["Raw"].toStyledString();
	}
	return ret;
}

Json::Value doGetRecord(RPC::Context&  context)
{
	uint160 nameInDB = beast::zero;
	std::vector<ripple::uint160> vecNameInDB;
	Json::Value ret = checkForSelect(context,nameInDB,vecNameInDB);
	if (ret.isMember(jss::error))
		return ret;

	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value result;
	Json::Value& tables_json = tx_json["Tables"];
	TxStore* pTxStore = &context.app.getTxStore();
	if (tables_json.size() == 1)//getTableStorage first_storage related
		pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);

	//db connection is null
	if (pTxStore->getDatabaseCon() == nullptr)
		return rpcError(rpcNODB);

	result = pTxStore->txHistory(context);

	//diff between the latest ledgerseq in db and the real newest ledgerseq
	result[jss::diff] = getDiff(context, vecNameInDB);;

	return result;
}

std::pair<std::vector<std::vector<Json::Value>>,std::string> doGetRecord2D(RPC::Context&  context)
{
	std::vector<std::vector<Json::Value>> result;
	uint160 nameInDB = beast::zero;
	std::vector<ripple::uint160> vecNameInDB;
	Json::Value ret = checkForSelect(context, nameInDB, vecNameInDB);
	if (ret.isMember(jss::error))
		return std::make_pair(result,ret[jss::error].asString());

	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value& tables_json = tx_json["Tables"];
	TxStore* pTxStore = &context.app.getTxStore();
	if (tables_json.size() == 1)//getTableStorage first_storage related
		pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);

	//db connection is null
	if (pTxStore->getDatabaseCon() == nullptr)
		return std::make_pair(result,"Db not configured.");

	return pTxStore->txHistory2d(context);
}

void buildRaw(Json::Value& condition, std::string& rule)
{
	Json::Value finalRaw;
	Json::Value finalObj(Json::objectValue);
	Json::Value jsonRule;
	Json::Reader().parse(rule, jsonRule);
	if (!jsonRule.isMember(jss::Condition))
		return;

	Json::Value arrayObj(Json::arrayValue);
	Json::Value specialCond(Json::arrayValue);
	if (condition.size() == 1)
	{
		if (condition[(Json::UInt)0].isMember("$order") || condition[(Json::UInt)0].isMember("$limit"))
		{
			specialCond.append(condition[(Json::UInt)0]);
		}
		else
		{
			arrayObj = condition;
		}
	}
	else
	{
		for (Json::UInt idx = 0; idx < condition.size(); idx++)
		{
			if (condition[idx].isMember("$order") || condition[idx].isMember("$limit"))
			{
				specialCond.append(condition[idx]);
			}
			else
			{
				arrayObj.append(condition[idx]);
			}
		}
	}

	//add special ones first
	for (Json::UInt idx = 0; idx < specialCond.size(); idx++)
	{
		finalRaw.append(specialCond[idx]);
	}

	Json::Value ruleCondition = jsonRule[jss::Condition];
	Json::Value finalRule;
	if (ruleCondition.isArray())
		finalRule["$or"] = ruleCondition;
	else
		finalRule = ruleCondition;

	Json::Value finalRawCond;
	if (arrayObj.size() > 1)
		finalRawCond["$or"] = arrayObj;
	else if(arrayObj.size() == 1)
		finalRawCond = arrayObj[(Json::UInt)0];

	Json::Value finalCondition(Json::arrayValue);
	if (arrayObj.size() > 0)
	{
		finalCondition.append(finalRawCond);
		finalCondition.append(finalRule);
		finalObj["$and"] = finalCondition;
		finalRaw.append(finalObj);
	}
	else
	{
		finalRaw.append(finalRule);
	}
	
	std::swap(finalRaw, condition);
}
//t_create:generate token & crypt raw
//t_assign:generate token
//r_insert&r_delete&r_update:crypt raw
Json::Value doPrepare(RPC::Context& context)
{
	auto& tx_json = context.params["tx_json"];
	auto ret = context.app.getTableAssistant().prepare(context.params["secret"].asString(), context.params["public_key"].asString(), tx_json,true);
	if (!ret.isMember("error_message"))
	{
		ret["status"] = "success";
		ret["tx_json"] = tx_json;
	}

	return ret;
}

Json::Value doGetUserToken(RPC::Context& context)
{
	Json::Value ret(Json::objectValue);
	auto& tx_json = context.params["tx_json"];
	
	auto tableName = tx_json["TableName"].asString();
	
	if (!tx_json.isMember(jss::Owner) || !tx_json.isMember("User"))
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "Owner or User is missing";
		return ret;
	}
	auto pOwner = ripple::parseBase58<AccountID>(tx_json["Owner"].asString());
	auto pAccount = ripple::parseBase58<AccountID>(tx_json["User"].asString());
	if (boost::none == pOwner || boost::none == pAccount)
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "Owner or User parse failed";
		return ret;
	}
	AccountID ownerID(*pOwner);
	AccountID accountID(*pAccount);

	bool bRet = false;
	ripple::Blob passWd;
	std::string sError;
	std::tie(bRet, passWd, sError) = context.ledgerMaster.getUserToken(accountID, ownerID, tableName);

	if (bRet)
	{
		ret[jss::status] = "success";
		ret["token"] = strHex(passWd);
	}
	else
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = sError;
	}

	return ret;
}
//////////////////////////////////////////////////////////////////////////
int getDiff(RPC::Context& context, const std::vector<ripple::uint160>& vec)
{
	int diff = 0;
	LedgerIndex txnseq, seq;
	uint256 txnhash, hash, txnupdatehash;
	if (context.app.getTxStoreDBConn().GetDBConn() == nullptr ||
		context.app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
	{
		return diff;
	}
	//current max-ledgerseq in network
	auto validIndex = context.app.getLedgerMaster().getValidLedgerIndex();
	for (auto iter = vec.begin(); iter != vec.end(); iter++)
	{
		//get ledgerseq in db
		context.app.getTableStatusDB().ReadSyncDB(to_string(*iter), txnseq, txnhash, seq, hash, txnupdatehash);
		int nTmpDiff = validIndex - seq;
		if (diff < nTmpDiff)
		{
			diff = nTmpDiff;
		}
	}
	return diff <= MAX_DIFF_TOLERANCE ? 0 : diff;
}

} // ripple
