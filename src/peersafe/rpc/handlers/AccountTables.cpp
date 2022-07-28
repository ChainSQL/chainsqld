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
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/rpc/TableUtils.h>

namespace ripple {

void
getTableInfo(
    RPC::JsonContext& context,
    std::shared_ptr<ReadView const> view,
    AccountID& ownerID,
    STObject const& table,
    bool bGetDetailInfo,
    Json::Value& ret)
{
    Json::Value tmp(Json::objectValue);
    auto nameInDB = table.getFieldH160(sfNameInDB);
    tmp[jss::nameInDB] = to_string(nameInDB);
    auto blob = table.getFieldVL(sfTableName);
    std::string str(blob.begin(), blob.end());
    tmp[jss::tablename] = str;
    LedgerIndex iInLedger = table.getFieldU32(sfCreateLgrSeq) + 1;
    tmp[jss::ledger_index] = iInLedger;
    uint256 txHash = table.getFieldH256(sfCreatedTxnHash);
    tmp[jss::tx_hash] = to_string(txHash);
    if (bGetDetailInfo)
    {
        auto tx = context.app.getMasterTransaction().fetch(txHash);
        if (tx)
        {
            auto pTx = tx->getSTransaction();
            std::vector<STTx> vecTxs =
                context.app.getMasterTransaction().getTxs(
                    *pTx, to_string(nameInDB), nullptr, iInLedger);
            for (auto it = vecTxs.begin(); it != vecTxs.end(); it++)
            {
                if ((*it).getFieldU16(sfOpType) == T_CREATE)
                {
                    if ((*it).isFieldPresent(sfRaw))
                    {
                        auto blob = (*it).getFieldVL(sfRaw);
                        std::string strRaw(blob.begin(), blob.end());
                        tmp[jss::raw] = strHex(strRaw);
                    }
                    uint256 ledgerHash =
                        context.app.getLedgerMaster().getHashBySeq(iInLedger);
                    tmp[jss::ledger_hash] = to_string(ledgerHash);

                    break;
                }
            }
        }
        tmp[jss::confidential] = isConfidential(*view, ownerID, str);

        auto ledger = context.app.getLedgerMaster().getLedgerBySeq(iInLedger);
        if (ledger != nullptr)
        {
            // tmp["create_time_human"] = to_string(ledger->info().closeTime);
            tmp["create_time"] =
                ledger->info().closeTime.time_since_epoch().count();
        }

        if (context.app.checkGlobalConnection())
        {
            auto pConn = context.app.getTxStoreDBConn().GetDBConn();
            uint160 nameInDB = table.getFieldH160(sfNameInDB);

            std::string sql_str = boost::str(
                boost::format(R"(SELECT count(*) from t_%s ;)") %
                to_string(nameInDB));
            boost::optional<int> count;
            LockedSociSession sql_session = pConn->checkoutDb();
            soci::statement st =
                (sql_session->prepare << sql_str, soci::into(count));
            try
            {
                bool dbret = st.execute(true);

                if (dbret && count)
                {
                    tmp[jss::count] = *count;
                    tmp["table_exist_inDB"] = true;
                }
            }
            catch (std::exception& /*e*/)
            {
                tmp["table_exist_inDB"] = false;
            }
        }
    }
    ret["tables"].append(tmp);
}

Json::Value
doGetAccountTables(RPC::JsonContext& context)
{
	Json::Value ret(Json::objectValue);
	auto& params = context.params;
	std::string accountStr;
    
	if (params.isMember(jss::account))
	{
		accountStr = params[jss::account].asString();
	}
	else
	{
		return RPC::missing_field_error(jss::account);
	}
	AccountID ownerID;
	auto jvAccepted = RPC::accountFromString(ownerID, accountStr, true);
	if (jvAccepted)
	{
		return jvAccepted;
	}
    
    std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);
    if (!ledger)
		return result;
    if (!ledger->exists(keylet::account(ownerID)))
	    return rpcError(rpcACT_NOT_FOUND);

	ret["tables"] = Json::Value(Json::arrayValue);

    bool bGetDetailInfo = false;
    if (context.params.isMember("detail") && context.params["detail"].asBool())
    {
        bGetDetailInfo = true;
    }

    forEachItem(
        *ledger,
        ownerID,
        [&bGetDetailInfo, &context, &ledger,&ownerID,&ret](
            std::shared_ptr<SLE const> const& sleNode) {
            if (sleNode->getType() == ltTABLE)
            {
                getTableInfo(
                    context,
                    ledger,
                    ownerID,
                    sleNode->getFieldObject(sfTableEntry),
                    bGetDetailInfo,
                    ret);
            }
            else if (sleNode->getType() == ltTABLELIST)
            {
                auto& aTables = sleNode->getFieldArray(sfTableEntries);
                for (auto& table : aTables)
                {
                    getTableInfo(
                        context, ledger, ownerID, table, bGetDetailInfo, ret);
                }
            }
        });

    return ret;
}

} // ripple
