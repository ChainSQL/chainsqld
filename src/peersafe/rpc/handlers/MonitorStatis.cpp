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
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/main/Application.h>
#include <peersafe/app/ledger/LedgerAdjust.h>
#include <peersafe/app/sql/TxnDBConn.h>

namespace ripple {

Json::Value
doMonitorStatis(RPC::JsonContext& context)
{
	std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);
    if (ledger == nullptr)
        return result;

    Json::Value ret(Json::objectValue);
    if (ledger != nullptr)
    {
        auto sleStatis = ledger->read(keylet::statis());
        if (sleStatis)
        {
            auto countSucc = sleStatis->getFieldU32(sfTxSuccessCountField);
            auto countFail = sleStatis->getFieldU32(sfTxFailureCountField);
            ret["txn_count"] = countSucc + countFail;
            ret["contract_count"] = sleStatis->getFieldU32(sfContractCreateCountField);
            ret["account_count"] = sleStatis->getFieldU32(sfAccountCountField);
        }
        else
        {
            TxnDBCon& connection = context.app.getTxnDB();
            
            auto countSucc = LedgerAdjust::getTxSucessCount(connection.checkoutDbRead());
            auto countFail = LedgerAdjust::getTxFailCount(connection.checkoutDb());
            ret["txn_count"] = countSucc + countFail;
            int contractCount = LedgerAdjust::getContractCreateCount(connection.checkoutDbRead());
            ret["contract_count"] = contractCount;
            int accountCount = LedgerAdjust::getAccountCount(connection.checkoutDbRead());
            if (accountCount > 1)
                accountCount = accountCount - contractCount;
            ret["account_count"] = accountCount;
        }
    }

    return ret;
}
}