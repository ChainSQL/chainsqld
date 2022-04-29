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


#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/Role.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/SecretKey.h>
#include <peersafe/schema/Schema.h>

#include <peersafe/app/storage/TableStorage.h> 
#include <peersafe/app/sql/STTx2SQL.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/app/table/TableStatusDB.h>
#include <peersafe/app/misc/ConnectionPool.h>
#include <iostream> 
#include <fstream>
#include <regex>
#include <ripple/basics/Slice.h>
#include <boost/format.hpp>

namespace ripple {

#define MAX_DIFF_TOLERANCE 3

void buildRaw(Json::Value& condition, std::string& rule);
int getDiff(RPC::JsonContext& context,TxStore& txStore, const std::vector<ripple::uint160>& vec);

//from rpc or http
Json::Value doRpcSubmit(RPC::JsonContext& context)
{
	Json::Value& tx_json(context.params["tx_json"]);
	auto ret = context.app.getTableAssistant().prepare(context.params["secret"].asString(),context.params["public_key"].asString(), tx_json);
	if (ret.isMember("error") || ret.isMember("error_message"))
		return ret;

    ret = doSubmit(context);
    return ret;
}

Json::Value doCreateFromRaw(RPC::JsonContext& context)
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

Json::Value checkForSelect(RPC::JsonContext&  context, uint160 nameInDB, std::vector<ripple::uint160> vecNameInDB)
{
	Json::Value ret(Json::objectValue);
	if (!context.params.isMember(jss::tx_json))
	{
		return RPC::missing_field_error(jss::tx_json);
	}
	Json::Value& tx_json(context.params["tx_json"]);
	if (!tx_json.isMember(jss::Owner))
	{
		return RPC::missing_field_error(jss::Owner);
	}
	if (!tx_json.isMember(jss::Account))
	{
		return RPC::missing_field_error(jss::Account);
	}

	AccountID ownerID;
    AccountID accountID;
	std::string ownerStr = tx_json[jss::Owner].asString();
	auto jvAcceptedOwner = RPC::accountFromString(ownerID, ownerStr, true);
	if (jvAcceptedOwner)
	{
		return jvAcceptedOwner;
	}

	std::string accountStr = tx_json[jss::Account].asString();
	auto jvAcceptedAccount = RPC::accountFromString(accountID, accountStr, true);
	if (jvAcceptedAccount)
	{
		return jvAcceptedAccount;
	}

	Json::Value &tables_json = tx_json[jss::Tables];
	if (!tables_json.isArray())
	{
		return RPC::make_error(rpcINVALID_PARAMS, "Field Tables is not array!");
	}

	auto j = context.app.journal("RPCHandler");
	JLOG(j.debug())
		<< "get record from tables: " << tx_json.toStyledString();

	std::list<std::string> listTableName;
	for (Json::UInt idx = 0; idx < tables_json.size(); idx++) 
	{
		Json::Value& e = tables_json[idx];
		Json::Value& v = e["Table"];
		if (!v.isObject())
		{
			return RPC::make_error(rpcINVALID_PARAMS, "Field Tables is not object!");
		}

		Json::Value tn = v["TableName"];
		if (!tn.isString())
		{
			return RPC::make_error(rpcINVALID_PARAMS, "Field TableName is not string!");
		}

		auto nameInDBGot = context.ledgerMaster.getNameInDB(context.ledgerMaster.getValidLedgerIndex(), ownerID, v["TableName"].asString());
		if (!nameInDBGot)
		{
			return rpcError(rpcTAB_NOT_EXIST);
		}
		listTableName.push_back(v["TableName"].asString());
		// NameInDB is optional
		if (v.isMember(jss::NameInDB))
		{
			//NameInDB is filled
			std::string sNameInDB = v[jss::NameInDB].asString();
			if (sNameInDB.length() > 0)
			{
				nameInDB = ripple::from_hex_text<ripple::uint160>(sNameInDB);
				if (nameInDBGot == nameInDB)
				{
					v["TableName"] = sNameInDB;
				}
				else
				{
					return rpcError(rpcNAMEINDB_NOT_MATCH);
				}
			}
			else
			{
				return RPC::invalid_field_error(jss::NameInDB);
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
	auto retPair = context.ledgerMaster.isAuthorityValid(accountID, ownerID, listTableName, lsfSelect);
	if (!retPair.first)
	{
		return rpcError(retPair.second);
	}

	std::string rule;
    auto ledger = context.ledgerMaster.getValidatedLedger();
	if (ledger)
	{
        auto tup = getTableEntry(*ledger, ownerID, listTableName.front());
        auto pEntry = std::get<1>(tup);
		if (pEntry)
            rule = STEntry::getOperationRule(*pEntry,R_GET);
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
				return RPC::invalid_field_error(jss::Raw);
			}
		}
		for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
		{
			auto& v = jsonRaw[idx];
			if (idx == 0)
			{
				if (!v.isArray())
				{
					return RPC::make_error(rpcINVALID_PARAMS, "Raw has a wrong format, first element must be an array");
				}
			}
			else
			{
				conditions.append(v);
			}
		}

		StringReplace(rule, "$account", tx_json["Account"].asString());
		if (conditions.isArrayOrNull())
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
Json::Value checkSig(RPC::JsonContext&  context)
{
	Json::Value ret(Json::objectValue);
	if (!context.params.isMember("publicKey"))
	{
		return RPC::missing_field_error("publicKey");
	}
	if (!context.params.isMember("signature"))
	{
		return RPC::missing_field_error("signature");
	}
	
	if (!context.params.isMember("signingData"))
	{
		return RPC::missing_field_error("signingData");
	}

	//tx_json should be equal to signingData
	std::string signingData = context.params["signingData"].asString();
	Json::Value json(Json::objectValue);
	Json::Reader().parse(signingData, json);
	auto& tx_json = context.params["tx_json"];
	if (json != tx_json)
	{
		return rpcError(rpcSIGN_NOT_MATCH);
	}

	//check for LedgerIndex
	auto valSeq = context.app.getLedgerMaster().getCurrentLedgerIndex();
	if (!tx_json.isMember("LedgerIndex"))
	{
		return RPC::missing_field_error("LedgerIndex");
	}
	auto seqInJson = tx_json["LedgerIndex"].asUInt();
	if (valSeq < seqInJson || valSeq - seqInJson > 5)
	{
		return RPC::invalid_field_error("LedgerIndex");
	}

	auto publicKey = context.params["publicKey"].asString();
	if (publicKey.empty())
	{
		return RPC::invalid_field_error("publicKey");
	}
	auto signatureHex = context.params["signature"].asString();
	if (signatureHex.empty())
	{
		return RPC::invalid_field_error("signature");
	}
	Blob spk;
	auto pub = strUnHex(publicKey);
	if (!pub)
	{
		return RPC::make_error(rpcINVALID_PARAMS, "Field publicKey should be hex string!");
	}
	spk = *pub;

	//check Account and publicKey
	AccountID const signingAcctIDFromPubKey =
		calcAccountID(PublicKey(makeSlice(spk)));
	if (tx_json[jss::Account].asString() != to_string(signingAcctIDFromPubKey))
	{
		return rpcError(rpcACT_NOT_MATCH_PUBKEY);
	}

	auto signature2 = strUnHex(signatureHex);
	if (!signature2)
	{
		return RPC::make_error(rpcINVALID_PARAMS, "Field signature should be hex string!");
	}

	auto signature = *signature2;
	if (publicKeyType(makeSlice(spk)))
	{
		bool success = verify(
			PublicKey(makeSlice(spk)),
			makeSlice(signingData),
			makeSlice(signature),
			false);
		if (!success)
		{
			return rpcError(rpcSIGNING_MALFORMED);
		}
		return ret;
	}
	else
	{
		return RPC::make_error(rpcPUBLIC_MALFORMED, "Field publicKey type error!");
	}
}

void getNameInDBSetInSql(std::string sql,std::set <std::string>& setTableNames)
{
	std::string prefix = "t_";
	//type of nameInDB: uint160
	int nTableNameLength =  2 * (160 / 8);
	int pos1 = sql.find(prefix);
	while (pos1 != std::string::npos){

		if ( pos1 + 2 + nTableNameLength <= sql.length() ){

			ripple::uint160 nameINDB;
			std::string str = sql.substr(pos1 + 2, nTableNameLength);

			// str must be  a hex_text type
			int pos2 = 0;
			if (nameINDB.SetHex(str)) {
				setTableNames.emplace(str);
				pos2 = pos1 + 2 + nTableNameLength;
			}
			else		pos2 = pos1 + 2;

			pos1 = sql.find(prefix, pos2);
		}
		else {
			break;
		}

	}
}



Json::Value getInfoByRPContext(RPC::JsonContext& context, std::string&sSql, AccountID& accountID)
{
	Json::Value& tx_json(context.params["tx_json"]);
	Json::Value ret;
	if (!tx_json.isMember(jss::Account))
	{
		return RPC::missing_field_error(jss::Account);
	}
	if (!tx_json.isMember("Sql"))
	{
		return RPC::missing_field_error("Sql");
	}

	std::string accountStr = tx_json[jss::Account].asString();

	auto jvAccepted = RPC::accountFromString(accountID, accountStr, true);
	if (jvAccepted)
	{
		return jvAccepted;
	}
	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (!ledger)
		return result;
	if (!ledger->exists(keylet::account(accountID)))
		return rpcError(rpcACT_NOT_FOUND);

	sSql = tx_json["Sql"].asString();
	if (sSql.empty())
	{
		return RPC::invalid_field_error("Sql");
	}

	return ret;

}


Json::Value getLedgerTableInfo(TxStore& txStore, const std::string& sql, std::set <std::string>& setDBTableNames,
							   std::set < std::pair<AccountID, std::string>  >& setOwnerID2TableName)
{

	getNameInDBSetInSql(sql, setDBTableNames);

	Json::Value ret(Json::objectValue);
	for (auto nameInDB : setDBTableNames)
	{
		std::string rule;

		Json::Value val = txStore.txHistory("select Owner,TableName from SyncTableState where TableNameInDB='" + nameInDB + "';");
		if (val.isMember(jss::error))
		{
			return val;
		}
		const Json::Value& lines = val[jss::lines];
		if (lines.isArray() == false || lines.size() != 1)
		{
			std::string errMsg = "Get value invalid from syncTableState, nameInDB=" + nameInDB;
			return RPC::make_error(rpcGET_VALUE_INVALID, errMsg);
		}

		const Json::Value & line = lines[0u];
		auto ownerID = ripple::parseBase58<AccountID>(line[jss::Owner].asString());
		setOwnerID2TableName.emplace(std::make_pair(*ownerID, line[jss::TableName].asString()));

	}
	return ret;

}


Json::Value getLedgerTableInfo(RPC::JsonContext& context,TxStore& txStore,AccountID& accountID, std::set <std::string>& setDBTableNames,
							   std::set < std::pair<AccountID,std::string>  >& setOwnerID2TableName)
{
	std::string sSql;
	Json::Value ret = getInfoByRPContext(context, sSql, accountID);
	if (ret.isMember(jss::error))
	{
		return ret;
	}

	getNameInDBSetInSql(sSql, setDBTableNames);
	for (auto nameInDB : setDBTableNames)
	{
		Json::Value ret;
		Json::Value val = txStore.txHistory("select Owner,TableName from SyncTableState where TableNameInDB='" + nameInDB + "';");
		if (val.isMember(jss::error))
		{
			return val;
		}
		const Json::Value& lines = val[jss::lines];
		if (lines.isArray() == false || lines.size() != 1)
		{
			std::string errMsg = "Get value invalid from syncTableState, nameInDB=" + nameInDB + " maybe not exist ";
			return RPC::make_error(rpcGET_VALUE_INVALID, errMsg);
		}
		const Json::Value & line = lines[0u];
		auto ownerID = ripple::parseBase58<AccountID>(line[jss::Owner].asString());

		setOwnerID2TableName.emplace(std::make_pair(*ownerID, line[jss::TableName].asString()));
	}

	return Json::Value();

}

// 

/**
	sql catenate OperateRule according to jss::condition

	@param rule               OperateRule 
	@param sql                sql string
	@param catenatedSql    a catenatedSql string

	@return Json::Value   
	   success:{};
	   error  :{"error":"Invalid field sql"}
*/

Json::Value catenateSqlAndOperateRule(const std::string& rule, const std::string& sql,std::string& catenatedSql) {

	Json::Value jsonRule;
	Json::Reader().parse(rule, jsonRule);

	bool bRule = jsonRule.isMember(jss::Condition);
	if (bRule){

		Json::Value& condition = jsonRule[jss::Condition];

		if (condition.isObject()) {

			Json::Value arr(Json::arrayValue);
			arr.append(condition);

			if (arr.isArray())
				condition = arr;
		}
		std::string sConditionSql;
		bool bSuccess = STTx2SQL::ConvertCondition2SQL(condition, sConditionSql);

		if (!bSuccess) {
			return RPC::invalid_field_error("Sql");
		}

		bool bHasNoKeyWords = ((sql.find("WHERE") == -1) && (sql.find("where") == -1));
		if (bHasNoKeyWords) {
			catenatedSql = (boost::format("%s where %s")
				% sql
				%sConditionSql).str();
		}
		else {
			catenatedSql = (boost::format("%s and %s")
				% sql
				%sConditionSql).str();
		}

	}
	else {

		catenatedSql = sql;

	}

	return Json::Value();	
}

Json::Value checkAuthForSql(RPC::JsonContext& context, AccountID& accountID, std::set< std::pair<AccountID, std::string>  >& setOwnerID2TableName)
{
	for (auto ownerID2TableName : setOwnerID2TableName)
	{
		//check the authority
		std::list<std::string> listTableName;
		listTableName.push_back(ownerID2TableName.second);
		auto retPair = context.ledgerMaster.isAuthorityValid(accountID, ownerID2TableName.first, listTableName, lsfSelect);
		if (!retPair.first)
		{
			return rpcError(retPair.second);
		}
	}
	return Json::Value();
}


// Reference to return 
Json::Value 
checkOperationRuleForSql(RPC::JsonContext& context, const std::string& sql,const std::set< std::pair<AccountID, std::string>  >& setOwnerID2TableName,std::string& catenatedSql)
{
	std::string rule;
	catenatedSql = sql;

	for (auto ownerID2TableName : setOwnerID2TableName)
	{
		auto ledger = context.ledgerMaster.getValidatedLedger();
		if (ledger)
		{
            auto tup = getTableEntry(*ledger,
						ownerID2TableName.first,
						ownerID2TableName.second); 
			auto pEntry = std::get<1>(tup);
			if (pEntry ) {
				rule = STEntry::getOperationRule(*pEntry,R_GET);
	
				// union queries && rule not supported
				if (!rule.empty() && setOwnerID2TableName.size() > 1)
				{
					return rpcError(rpcSQL_MULQUERY_NOT_SUPPORT);
				}

				Json::Value ret = catenateSqlAndOperateRule(rule, sql, catenatedSql);
				if (ret.isMember(jss::error))
					return ret;

			}
		}
	}
	return Json::Value();
}



Json::Value 
checkOperationRuleForSqlUser(RPC::JsonContext& context,const AccountID& accountID, const std::set< std::pair<AccountID, std::string>  >& setOwnerID2TableName, std::string& catenatedSql)
{
	Json::Value retJson;

	Json::Value& tx_json(context.params["tx_json"]);

	std::string sql = tx_json["Sql"].asString();
	catenatedSql    = sql;

	for (auto ownerID2TableName : setOwnerID2TableName)
	{
		std::string rule;
		auto ledger = context.ledgerMaster.getValidatedLedger();
		if (ledger)
		{
			//judge if account is activated
			auto key = keylet::account(accountID);
			if (!ledger->exists(key))
			{
				return rpcError(rpcACT_NOT_FOUND);
			}
			auto tup = getTableEntry(
						*ledger,
                        ownerID2TableName.first,
                        ownerID2TableName.second);
            auto pEntry = std::get<1>(tup);
			if (pEntry)
				rule = STEntry::getOperationRule(*pEntry,R_GET);
		}

		// union queries && rule not supported
		if (!rule.empty() && setOwnerID2TableName.size() > 1)
		{
			return rpcError(rpcSQL_MULQUERY_NOT_SUPPORT);
		}


		Json::Value ret = catenateSqlAndOperateRule(rule, sql,catenatedSql);
		if (ret.isMember(jss::error))
			return ret;

		retJson = ret;

	}
	return retJson;
}


Json::Value
doGetRecord(RPC::JsonContext& context)
{
    Json::Value ret = checkSig(context);
    if (ret.isMember(jss::error))
        return ret;

    uint160 nameInDB = beast::zero;
    std::vector<ripple::uint160> vecNameInDB;
    ret = checkForSelect(context, nameInDB, vecNameInDB);
    if (ret.isMember(jss::error))
        return ret;

    // db connection is null
    if (!isDBConfigured(context.app))
    {
        return rpcError(rpcNODB);
    }

    auto unit = context.app.getConnectionPool().getAvailable();
    TxStore* pTxStore = &(*unit->store_);
    // Json::Value& tx_json(context.params["tx_json"]);
    // Json::Value& tables_json = tx_json["Tables"];
    // if (tables_json.size() == 1)//getTableStorage first_storage related
    //{
    //	pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);
    //	unit->unlock();
    //}
    try
    {
        ret = pTxStore->txHistory(context);
        if (!ret.isMember(jss::error))
        {
            // diff between the latest ledgerseq in db and the real newest
            // ledgerseq
            ret[jss::diff] = getDiff(context, *pTxStore, vecNameInDB);
        }
        context.app.getConnectionPool().releaseConnection(unit);
    }
    catch (std::exception const& e)
    {
        JLOG(context.app.journal("RPCHandler").error())
            << "doGetRecord exception" << e.what();
        context.app.getConnectionPool().releaseConnection(unit);
    }

    return ret;
}

Json::Value queryBySql(TxStore& txStore,std::string& sql)
{
	Json::Value ret(Json::objectValue);
	sql.erase(0, sql.find_first_not_of(' '));
	size_t posSpace = sql.find_first_of(' ');
	std::string firstWord = sql.substr(0, posSpace);
	if (toUpper(firstWord) != "SELECT")
	{
		return rpcError(rpcSQL_SELECT_ONLY, ret);
	}
	ret = txStore.txHistory(sql);
	return ret;
}

Json::Value checkTableExistOnChain(RPC::JsonContext&  context, std::set < std::pair<AccountID, std::string>  >& setOwnerID2TableName)
{
	Json::Value ret(Json::objectValue);

	for ( auto ownerID2TableName : setOwnerID2TableName)
	{

		//check table exist
		auto ledger = context.ledgerMaster.getValidatedLedger();
		if (ledger)
		{
			auto tup = getTableEntry(
				*ledger,
				ownerID2TableName.first,
				ownerID2TableName.second);
            auto pEntry = std::get<1>(tup);
            if (pEntry == nullptr)
            {
                return rpcError(rpcTAB_NOT_EXIST);
            }
		}
	}
	return ret;
}

Json::Value doGetRecordBySql(RPC::JsonContext&  context)
{
	Json::Value ret(Json::objectValue);
	//db connection is null
	if (!isDBConfigured(context.app))
		return rpcError(rpcNODB);

	if (!context.params.isMember(jss::sql))
	{
		return RPC::missing_field_error(jss::sql);
	}		
	auto sql = context.params[jss::sql].asString();
	if (sql.empty())
	{
		return RPC::invalid_field_error(jss::sql);
	}

	auto unit = context.app.getConnectionPool().getAvailable();
	TxStore& txStore = *unit->store_;
	std::set <std::string> setNameInDB;
	std::set < std::pair<AccountID, std::string>  > setOwnerID2TableName;
    try
    {
        ret = getLedgerTableInfo(txStore, sql, setNameInDB, setOwnerID2TableName);

        // check table exist on chain
        ret = checkTableExistOnChain(context, setOwnerID2TableName);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        std::string catenatedSql;
        ret = checkOperationRuleForSql(
            context, sql, setOwnerID2TableName, catenatedSql);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        ret = queryBySql(txStore, catenatedSql);

        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        // diff between the latest ledgerseq in db and the real newest ledgerseq
        std::vector<ripple::uint160> vecNameInDB;
        for (std::set<std::string>::iterator iter = setNameInDB.begin();
             iter != setNameInDB.end();
             ++iter)
        {
            vecNameInDB.push_back(
                ripple::from_hex_text<ripple::uint160>(to_string(*iter)));
        }

        ret[jss::diff] = getDiff(context, txStore, vecNameInDB);
        context.app.getConnectionPool().releaseConnection(unit);
	}
    catch (std::exception const& e)
    {
        context.app.getConnectionPool().releaseConnection(unit);
        JLOG(context.app.journal("RPCHandler").error())
            << "doGetRecordBySql exception" << e.what();
	}
	

	return ret;
}

Json::Value doGetRecordBySqlUser(RPC::JsonContext& context)
{
	//check signature
	Json::Value ret = checkSig(context);
	if (ret.isMember(jss::error))
		return ret;

	//db connection is null
	if (!isDBConfigured(context.app))
		return rpcError(rpcNODB);

	auto unit = context.app.getConnectionPool().getAvailable();
	TxStore& txStore = *unit->store_;
	
	AccountID accountID;
	std::set <std::string> setNameInDB;
	std::set < std::pair<AccountID, std::string>  > setOwnerID2TableName;
    try
    {
        ret = getLedgerTableInfo(
            context, txStore, accountID, setNameInDB, setOwnerID2TableName);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        // check table authority
        ret = checkAuthForSql(context, accountID, setOwnerID2TableName);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        std::string catenatedSql;
        ret = checkOperationRuleForSqlUser(
            context, accountID, setOwnerID2TableName, catenatedSql);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        ret = queryBySql(txStore, catenatedSql);
        if (ret.isMember(jss::error))
        {
            context.app.getConnectionPool().releaseConnection(unit);
            return ret;
        }

        // diff between the latest ledgerseq in db and the real newest ledgerseq
        std::vector<ripple::uint160> vecNameInDB;
        for (std::set<std::string>::iterator iter = setNameInDB.begin();
             iter != setNameInDB.end();
             ++iter)
        {
            vecNameInDB.push_back(
                ripple::from_hex_text<ripple::uint160>(to_string(*iter)));
        }

        ret[jss::diff] = getDiff(context, txStore, vecNameInDB);
        context.app.getConnectionPool().releaseConnection(unit);
    }
    catch (std::exception const& e)
    {
        context.app.getConnectionPool().releaseConnection(unit);
        JLOG(context.app.journal("RPCHandler").error())
            << "doGetRecordBySqlUser exception" << e.what();
    }	

	return ret;
}

//Get record,will keep column order consistent with the order the table created.
std::pair<std::vector<std::vector<Json::Value>>,std::string> doGetRecord2D(RPC::JsonContext&  context)
{
	std::vector<std::vector<Json::Value>> result;
	uint160 nameInDB = beast::zero;
	std::vector<ripple::uint160> vecNameInDB;
	Json::Value ret = checkForSelect(context, nameInDB, vecNameInDB);
	if (ret.isMember(jss::error))
		return std::make_pair(result,ret[jss::error_message].asString());

	//db connection is null
	if (!isDBConfigured(context.app))
		return std::make_pair(result, "Db not configured.");


	auto unit = context.app.getConnectionPool().getAvailable();
    TxStore* pTxStore = &(*unit->store_);
    //Json::Value& tx_json(context.params["tx_json"]);
    //Json::Value& tables_json = tx_json["Tables"];
	//if (tables_json.size() == 1)//getTableStorage first_storage related
	//{
	//	pTxStore = &context.app.getTableStorage().GetTxStore(nameInDB);
	//	unit->unlock();
	//}
    try
    {
        auto retVec = pTxStore->txHistory2d(context);
        context.app.getConnectionPool().releaseConnection(unit);
        return retVec;
    }
    catch (std::exception const& e)
    {
        context.app.getConnectionPool().releaseConnection(unit);
        JLOG(context.app.journal("RPCHandler").error())
            << "doGetRecord2D exception" << e.what();
        return std::make_pair(result, "Exception occurred.");
    }	
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
Json::Value doPrepare(RPC::JsonContext& context)
{
	auto& tx_json = context.params["tx_json"];
	auto ret = context.app.getTableAssistant().prepare(context.params["secret"].asString(), context.params["public_key"].asString(), tx_json,true);
	if (!ret.isMember(jss::error))
	{
		ret["tx_json"] = tx_json;
	}

	return ret;
}

Json::Value doGetUserToken(RPC::JsonContext& context)
{
	Json::Value ret(Json::objectValue);
	auto& tx_json = context.params["tx_json"];
	
	if (!tx_json.isMember(jss::TableName))
	{
		return RPC::missing_field_error(jss::TableName);
	}
	auto tableName = tx_json[jss::TableName].asString();
	if (tableName.empty())
	{
		return RPC::invalid_field_error(jss::TableName);
	}
	std::string ownerStr = "";
	std::string userStr = "";
	//check owner
	if (tx_json.isMember(jss::Owner) && !tx_json[jss::Owner].asString().empty())
	{
		ownerStr = tx_json[jss::Owner].asString();
	}
	else 
	{
		return RPC::missing_field_error(jss::Owner);
	}
	//check user
	if (tx_json.isMember(jss::User) && !tx_json[jss::User].asString().empty())
	{
		userStr = tx_json[jss::User].asString();
	}
	else
	{
		return RPC::missing_field_error(jss::User);
	}

	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (!ledger)
		return result;
	
	AccountID ownerID;
	AccountID userID;
	auto jvAcceptedOwner = RPC::accountFromString(ownerID, ownerStr, true);
	if (jvAcceptedOwner)
		return jvAcceptedOwner;
	if (!ledger->exists(keylet::account(ownerID)))
		return rpcError(rpcACT_NOT_FOUND);
	auto jvAcceptedUser = RPC::accountFromString(userID, userStr, true);
	if (jvAcceptedUser)
		return jvAcceptedUser;
	if (!ledger->exists(keylet::account(userID)))
		return rpcError(rpcACT_NOT_FOUND);

	bool bRet = false;
	ripple::Blob passWd;
	error_code_i errCode;
    std::tie(bRet, passWd, errCode) = context.ledgerMaster.getUserToken(
        ledger ,userID, ownerID, tableName);

	if (bRet)
	{
		ret["token"] = strHex(passWd);
	}
	else
	{
		return rpcError(errCode);
	}

	return ret;
}
//////////////////////////////////////////////////////////////////////////
bool 
readSyncSeq(
    RPC::JsonContext& context,
    TxStore& txStore,
    std::string nameInDB,
    LedgerIndex& seq)
{
    bool ret = false;
    try
    {
        LockedSociSession sql_session = txStore.getDatabaseCon()->checkoutDb();

        static std::string const prefix(
            R"(select LedgerSeq from SyncTableState
            WHERE )");

        std::string sql = boost::str(
            boost::format(prefix + (R"(TableNameInDB = '%s';)")) % nameInDB);

        boost::optional<std::string> LedgerSeq;

        soci::statement st =
            (sql_session->prepare << sql,
             soci::into(LedgerSeq));

        bool dbret = st.execute(true);

        if (dbret)
        {
            if (LedgerSeq && !LedgerSeq.value().empty())
                seq = std::stoi(LedgerSeq.value());
            ret = true;
        }
    }
    catch (std::exception const& e)
    {
        JLOG(context.app.journal("RPCHandler").error())
            << "ReadSyncDB exception" << e.what();
    }
    return ret;
}

int
getDiff(
    RPC::JsonContext& context,
    TxStore& txStore,
	const std::vector<ripple::uint160>& vec)
{
	int diff = 0;
	LedgerIndex seq;
	uint256 txnhash, hash, txnupdatehash;
	//current max-ledgerseq in network
	auto validIndex = context.app.getLedgerMaster().getValidLedgerIndex();
	for (auto iter = vec.begin(); iter != vec.end(); iter++)
	{
		//get ledgerseq in db
        readSyncSeq(
            context,
            txStore,
            to_string(*iter),
            seq);
		int nTmpDiff = validIndex - seq;
		if (diff < nTmpDiff)
		{
			diff = nTmpDiff;
		}
	}
	return diff <= MAX_DIFF_TOLERANCE ? 0 : diff;
}

} // ripple
