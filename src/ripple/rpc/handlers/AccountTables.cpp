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
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/protocol/STEntry.h>

namespace ripple {
Json::Value doGetAccountTables(RPC::Context&  context)
{
	Json::Value ret(Json::objectValue);
    

	if (context.params["account"].asString().empty())
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "Account  is null";
		return ret;
	}

	auto pOwnerId = ripple::parseBase58<AccountID>(context.params["account"].asString());
	if (pOwnerId == boost::none)
	{
		ret[jss::status] = "error";
		ret[jss::error_message] = "account parse failed";
		return ret;
	}

    bool bGetDetailInfo = false;
    if (context.params.isMember("detail") && context.params["detail"].asBool())
    {
        bGetDetailInfo = true;
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
                    auto  tx = context.app.getMasterTransaction().fetch(txHash, true);
					if (tx)
					{
						auto  pTx = tx->getSTransaction();
                        std::vector<STTx> vecTxs = context.app.getMasterTransaction().getTxs(*pTx, to_string(nameInDB), nullptr, iInLedger);
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
                                uint256 ledgerHash = context.app.getLedgerMaster().getHashBySeq(iInLedger);
                                tmp[jss::ledger_hash] = to_string(ledgerHash);

                                break;
                            }
                        }						
					}
					STEntry* pEntry = (STEntry*)(&table);
					tmp[jss::confidential] = pEntry->isConfidential();

					if (context.app.getTxStoreDBConn().GetDBConn() != nullptr &&
						context.app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() != nullptr)
					{
						auto pConn = context.app.getTxStoreDBConn().GetDBConn();
						uint160 nameInDB = table.getFieldH160(sfNameInDB);

						std::string sql_str = boost::str(boost::format(
							R"(SELECT count(*) from t_%s ;)")
							% to_string(nameInDB));
						boost::optional<int> count;
						LockedSociSession sql_session = pConn->checkoutDb();
						soci::statement st = (sql_session->prepare << sql_str
							, soci::into(count));
						try {
							bool dbret = st.execute(true);

							if (dbret && count)
							{
								tmp[jss::count] = *count;
							}
						}
						catch (std::exception& e) {
							tmp[jss::error_message] = e.what();
						}
					}
                }
				ret["tables"].append(tmp);
            }
        }
        else
        {
            ret[jss::status] = "error";
            ret[jss::message] = "There is no table in this account!";
        }
    }
    else
    {
        ret[jss::status] = "error!";
        ret[jss::message] = "There is no table in this account!";
    }

    return ret;
}

} // ripple
