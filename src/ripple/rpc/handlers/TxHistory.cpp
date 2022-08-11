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

#include <peersafe/schema/Schema.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/SociDB.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/Role.h>
#include <boost/format.hpp>

namespace ripple {

std::vector<std::string>
parseTypes(Json::Value const& jvArray)
{
    std::vector<std::string> result;
    for (auto const& jv : jvArray)
    {
        if (!jv.isString())
            return result;
        if (jv.asString() != "EnableAmendment" &&
            jv.asString() != "SetFee" &&
            jv.asString() != "UNLModify")
        result.push_back(jv.asString());
    }
    return result;
}

// {
//   "start": <index>
//   "types": [type1, type2...]
// }
Json::Value
doTxHistory(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return rpcError(rpcNOT_ENABLED);

    context.loadType = Resource::feeMediumBurdenRPC;

    if (!context.params.isMember(jss::start))
        return rpcError(rpcINVALID_PARAMS);

    unsigned int startIndex = context.params[jss::start].asUInt();

    if ((startIndex > 10000) && (!isUnlimited(context.role)))
        return rpcError(rpcNO_PERMISSION);

    std::vector<std::string> txTypes;
    if (context.params.isMember(jss::types))
    {
        if (!context.params[jss::types].isArray())
            return RPC::make_error(
                rpcINVALID_PARAMS, "Field types is not array");
        if (context.params[jss::types].size() > 0)
        {
            if (txTypes = parseTypes(context.params[jss::types]);
                txTypes.size() <= 0)
            {
                return RPC::make_error(
                    rpcINVALID_PARAMS, "Field types is malformed");
            }
        }
    }

    Json::Value obj;
    Json::Value txs;

    obj[jss::index] = startIndex;

    std::string sql;

    if (txTypes.size() <= 0)
    {
        sql = boost::str(
            boost::format(
                "SELECT TransID "
                "FROM Transactions WHERE (TransType != 'EnableAmendment' "
                "AND TransType != 'SetFee' AND TransType != 'UNLModify') "
                "ORDER BY LedgerSeq desc LIMIT %u,20;") %
            startIndex);
    }
    else
    {
        std::string cond;
        for (std::size_t idx = 0; idx < txTypes.size(); idx++)
        {
            cond +=
                boost::str(boost::format("TransType = '%s'") % txTypes[idx]);
            if (idx != txTypes.size() - 1)
                cond += " OR ";
        }

        sql = boost::str(
            boost::format("SELECT TransID "
                          "FROM Transactions WHERE (%s) "
                          "ORDER BY LedgerSeq desc LIMIT %u,20;") %
            cond % startIndex);
    }

    {
        auto db = context.app.getTxnDBCHECK().checkoutDbRead();

        boost::optional<std::string> stxnHash;
        soci::statement st = (db->prepare << sql, soci::into(stxnHash));
        st.execute();

        while (st.fetch())
        {
            uint256 txID = from_hex_text<uint256>(stxnHash.value());
            auto txn = context.app.getMasterTransaction().fetch(txID);
            if (!txn)
            {
                continue;
            }

            txs.append(txn->getJson(JsonOptions::none));
        }
    }

    obj[jss::txs] = txs;

    return obj;
}

}  // namespace ripple
