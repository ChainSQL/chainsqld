//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/rpc/impl/TableAssistant.h>

namespace ripple {
Json::Value doGetAccountTables(RPC::Context&  context)
{
	Json::Value ret(Json::objectValue);
    
    Json::Value& tx_json(context.params["tx_json"]);

	if (tx_json["account"].asString().empty())
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "Account  is null";
		return ret;
	}

	auto pOwnerId = ripple::parseBase58<AccountID>(tx_json["account"].asString());
	if (pOwnerId == boost::none)
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "account parse failed";
		return ret;
	}
    AccountID ownerID(*pOwnerId);
    auto key = keylet::table(ownerID);

    auto ledger = context.app.getLedgerMaster().getValidatedLedger();
    auto tablesle = ledger->read(key);
    if (tablesle)
    {
        auto & aTables = tablesle->getFieldArray(sfTableEntries);
        if (aTables.size() > 0)
        {
            ret[jss::status] = "success";
            for (auto table : aTables)
            {
                if (table.getFieldU8(sfDeleted) == 0)
                {
                    Json::Value tmp(Json::objectValue);
                    tmp[jss::NameInDB] = to_string(table.getFieldH160(sfNameInDB));
                    auto blob = table.getFieldVL(sfTableName);
                    std::string str(blob.begin(), blob.end());
                    tmp[jss::TableName] = str;
                    ret["tx_json"].append(tmp);
                }
            }
        }
        else
        {
            ret[jss::status] = "error";
            ret[jss::message] = "this is no table in this account!";
        }
    }
    else
    {
        ret[jss::status] = "error!";
        ret[jss::message] = "this is no table in this account!";
    }

    return ret;
}

} // ripple
