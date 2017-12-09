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
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/protocol/digest.h>
#include <peersafe/app/sql/TxStore.h>

namespace ripple {
Json::Value doGetCheckHash(RPC::Context&  context)
{
	Json::Value ret(Json::objectValue);
	Json::Value& tx_json(context.params["tx_json"]);

	if (tx_json["Account"].asString().empty() || tx_json["TableName"].asString().empty())
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "Account or TableName is null";
		return ret;
	}

	auto accountIdStr = tx_json["Account"].asString();
	auto tableNameStr = tx_json["TableName"].asString();

	ripple::AccountID accountID;
	auto jvAccepted = RPC::accountFromString(accountID, accountIdStr, false);

	if (jvAccepted)
		return jvAccepted;

	//first,we query from ledgerMaster
	auto retPair = context.ledgerMaster.getLatestTxCheckHash(accountID, tableNameStr);
	auto txCheckHash = retPair.first;
	if (txCheckHash.isZero()) //not exist,then generate nameInDB
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = retPair.second;
	}	

	ret[jss::status] = "success";
	ret["txCheckHash"] = to_string(txCheckHash);

	return ret;
}

} // ripple
