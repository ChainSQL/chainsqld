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
#include <regex>
#include <ripple/basics/Slice.h>

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
	if (!context.params.isMember("tx_json"))
	{
		return generateError("field tx_json is empty!");
	}
	Json::Value& tx_json(context.params["tx_json"]);
	if (tx_json["Owner"].asString().empty())
	{
		return generateError("field Owner is empty!");
	}
	auto ownerID = ripple::parseBase58<AccountID>(tx_json["Owner"].asString());
	if (ownerID == boost::none)
	{
		return generateError("field Owner is wrong!");
	}

	if (tx_json["Account"].asString().empty())
	{
		return generateError("field Account is empty!");
	}
	auto accountID = ripple::parseBase58<AccountID>(tx_json["Account"].asString());
	if (accountID == boost::none)
	{
		return generateError("field Account is wrong!");
	}

	Json::Value &tables_json = tx_json["Tables"];
	if (!tables_json.isArray())
	{
		return generateError("field Tables is not array!");
	}

	auto j = context.app.journal("RPCHandler");
	JLOG(j.debug())
		<< "get record from tables: " << tx_json.toStyledString();

	std::list<std::string> listTableName;
	for (Json::UInt idx = 0; idx < tables_json.size(); idx++) {
		Json::Value& e = tables_json[idx];
		Json::Value& v = e["Table"];
		if (!v.isObject())
		{
			return generateError("field Table is not object!");
		}

		Json::Value tn = v["TableName"];
		if (!tn.isString())
		{
			return generateError("field TableName is not string!");
		}

		auto nameInDBGot = context.ledgerMaster.getNameInDB(context.ledgerMaster.getValidLedgerIndex(), *ownerID, v["TableName"].asString());
		if (!nameInDBGot)
		{
			return generateError("can't get TableName in DB ,please check field tablename!");
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
					return generateError("please make sure the given 'NameInDB' in accordance with the 'TableName!");
				}
			}
			else
			{
				return generateError("field NameInDB is empty!");
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
		return generateError(retPair.second);
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
			return generateError("account not valid!");
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
				return generateError("Raw not valid!");
			}
		}
		for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
		{
			auto& v = jsonRaw[idx];
			if (idx == 0)
			{
				if (!v.isArray())
				{
					return generateError("Raw has a wrong format ,first element must be an array");
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
Json::Value checkSig(RPC::Context&  context)
{
	Json::Value ret(Json::objectValue);
	if (!context.params.isMember("publicKey"))
	{
		return generateError("field publicKey is empty!");
	}
	if (!context.params.isMember("signature"))
	{
		return generateError("field signature is empty!");
	}
	
	if (!context.params.isMember("signingData"))
	{
		return generateError("field signingData is empty!");
	}

	//tx_json should be equal to signingData
	std::string signingData = context.params["signingData"].asString();
	Json::Value json(Json::objectValue);
	Json::Reader().parse(signingData, json);
	auto& tx_json = context.params["tx_json"];
	if (json != tx_json)
	{
		return generateError("signing data does not match tx_json!");
	}

	//check for LedgerIndex
	auto valSeq = context.app.getLedgerMaster().getValidatedLedger()->info().seq;
	if (!tx_json.isMember("LedgerIndex"))
	{
		return generateError("field LedgerIndex is empty!");
	}
	auto seqInJson = tx_json["LedgerIndex"].asUInt();
	if (valSeq - seqInJson < 0 || valSeq - seqInJson > 3)
	{
		return generateError("LedgerIndex in tx_json is not valid!");
	}

	auto publicKey = context.params["publicKey"].asString();
	auto signatureHex = context.params["signature"].asString();
	Blob spk;
	auto retPair = strUnHex(publicKey);
	if (!retPair.second)
	{
		return generateError("field publicKey should be hex string!");
	}
	spk = retPair.first;

	//check Account and publicKey
	AccountID const signingAcctIDFromPubKey =
		calcAccountID(PublicKey(makeSlice(spk)));
	if (tx_json[jss::Account].asString() != to_string(signingAcctIDFromPubKey))
	{
		return generateError("Account in tx_json doesn't match publicKey for signing!");
	}

	retPair = strUnHex(signatureHex);
	if (!retPair.second)
	{
		return generateError("field signature should be hex string!");
	}

	auto signature = retPair.first;
	if (publicKeyType(makeSlice(spk)))
	{
		bool success = verify(
			PublicKey(makeSlice(spk)),
			makeSlice(signingData),
			makeSlice(signature),
			false);
		if (!success)
		{
			return generateError("check signature failed!");
		}
		return ret;
	}
	else
	{
		return generateError("publicKey type error!");
	}
}

Json::Value checkAuthForSql(RPC::Context& context)
{
	TxStore& txStore = context.app.getTxStore();
	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value ret;
	if (!tx_json.isMember("Account"))
	{
		ret[jss::error] = "Missing field Account!";
		return ret;
	}
	if (!tx_json.isMember("Sql"))
	{
		ret[jss::error] = "Missing field Sql!";
		return ret;
	}

	auto accountID = ripple::parseBase58<AccountID>(tx_json["Account"].asString());
	if (accountID == boost::none)
	{
		ret[jss::error] = "field Account is wrong!";
		return ret;
	}

	std::string sql = tx_json["Sql"].asString();
	std::string prefix = "t_";
	//type of nameInDB: uint160
	int nTableNameLength = prefix.length() + 2 * (160/8);
	std::set <std::string> setTableNames;
	int pos1 = sql.find(prefix);
	int pos2 = 0;
	while (pos1 != std::string::npos)
	{
		int pos2 = sql.find(" ", pos1);
		if (pos2 == std::string::npos)
			pos2 = sql.length();
		if (pos2 - pos1 == nTableNameLength)
		{
			std::string str = sql.substr(pos1 + 2, pos2 - pos1);
			setTableNames.emplace(str);
		}

		pos1 = sql.find(prefix, pos2);
	}

	for (auto nameInDB : setTableNames)
	{
		Json::Value val = txStore.txHistory("select Owner,TableName from SyncTableState where TableNameInDB='" + nameInDB +"';");
		if (val.isMember(jss::error))
		{
			return val;
		}
		const Json::Value& lines = val[jss::lines];
		if (lines.isArray() == false || lines.size() != 1)
		{
			ret[jss::error] = "Return value not valid while select Owner,TableName from SyncTableState!";
			return ret;
		}
		const Json::Value & line = lines[0u];

		auto ownerID = ripple::parseBase58<AccountID>(line[jss::Owner].asString());
		
		//check the authority
		std::list<std::string> listTableName;
		listTableName.push_back(line[jss::TableName].asString());
		auto retPair = context.ledgerMaster.isAuthorityValid(*accountID, *ownerID, listTableName, lsfSelect);
		if (!retPair.first)
		{
			ret[jss::error] = retPair.second;
			return ret;
		}
	}
	return ret;
}

Json::Value doGetRecord(RPC::Context&  context)
{
	Json::Value ret = checkSig(context); 
	if (ret.isMember(jss::status) && ret[jss::status].asString() == "error")
		return ret;

	uint160 nameInDB = beast::zero;
	std::vector<ripple::uint160> vecNameInDB;
	ret = checkForSelect(context, nameInDB, vecNameInDB);
	if (ret.isMember(jss::status) && ret[jss::status].asString() == "error")
		return ret;

	//db connection is null
	if (!isDBConfigured(context.app))
		return generateError("Get db connection error,maybe db not configured.");

	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value result;
	Json::Value& tables_json = tx_json["Tables"];
	TxStore* pTxStore = &context.app.getTxStore();
	if (tables_json.size() == 1)//getTableStorage first_storage related
		pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);

	result = pTxStore->txHistory(context);

	if (result.isMember(jss::error))
	{
		return generateError(result[jss::error].asString());
	}
	else
	{
		//diff between the latest ledgerseq in db and the real newest ledgerseq
		result[jss::diff] = getDiff(context, vecNameInDB);
		return result;
	}	
}

Json::Value queryBySql(TxStore& txStore,std::string& sql)
{
	Json::Value ret(Json::objectValue);
	if (sql.empty())
	{
		return generateError("Field sql is empty!");
	}

	size_t posSpace = sql.find_first_of(' ');
	std::string firstWord = sql.substr(0, posSpace);
	if (toUpper(firstWord) != "SELECT")
	{
		return generateError("You can only query table data,first word should be select!");
	}
	ret = txStore.txHistory(sql);
	if (ret.isMember("error"))
	{
		return generateError(ret[jss::error].asString());
	}
	else
	{
		return ret;
	}	
}

Json::Value doGetRecordBySql(RPC::Context&  context)
{
	Json::Value ret(Json::objectValue);
	//db connection is null
	if (!isDBConfigured(context.app))
		return generateError("Get db connection error,maybe db not configured.");

	if (!context.params.isMember("sql"))
	{
		return generateError("Missing field sql!");
	}		
	auto sql = context.params["sql"].asString();
	return queryBySql(context.app.getTxStore(),sql);
}

Json::Value doGetRecordBySqlUser(RPC::Context& context)
{
	//check signature
	Json::Value ret = checkSig(context);
	if (ret.isMember(jss::status) && ret[jss::status].asString() == "error")
		return ret;

	//db connection is null
	if (!isDBConfigured(context.app))
		return generateError("Get db connection error,maybe db not configured.");

	Json::Value& tx_json(context.params["tx_json"]);
	//check table authority
	ret = checkAuthForSql(context);
	if (ret.isMember(jss::status) && ret[jss::status].asString() == "error")
	{
		return ret;
	}

	// get result
	auto sql = tx_json["Sql"].asString();
	return queryBySql(context.app.getTxStore(), sql);
}

//Get record,will keep column order consistent with the order the table created.
std::pair<std::vector<std::vector<Json::Value>>,std::string> doGetRecord2D(RPC::Context&  context)
{
	std::vector<std::vector<Json::Value>> result;
	uint160 nameInDB = beast::zero;
	std::vector<ripple::uint160> vecNameInDB;
	Json::Value ret = checkForSelect(context, nameInDB, vecNameInDB);
	if (ret.isMember(jss::status) && ret[jss::status].asString() == "error")
		return std::make_pair(result,ret[jss::error_message].asString());

	//db connection is null
	if (!isDBConfigured(context.app))
		return std::make_pair(result, "Db not configured.");

	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value& tables_json = tx_json["Tables"];
	TxStore* pTxStore = &context.app.getTxStore();
	if (tables_json.size() == 1)//getTableStorage first_storage related
		pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);

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
		return generateError("Owner or User is missing");
	}
	auto pOwner = ripple::parseBase58<AccountID>(tx_json["Owner"].asString());
	auto pAccount = ripple::parseBase58<AccountID>(tx_json["User"].asString());
	if (boost::none == pOwner || boost::none == pAccount)
	{
		return generateError("Owner or User parse failed");
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
		return generateError(sError);
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
