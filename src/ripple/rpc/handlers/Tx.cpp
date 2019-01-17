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

#include <BeastConfig.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/beast/core/LexicalCast.h>
#include <boost/optional/optional_io.hpp>
#include <peersafe/rpc/TableUtils.h>
#include <chrono>

namespace ripple {

    // {
    //   transaction: <hex>
    // }
    std::pair<std::vector<std::shared_ptr<STTx>>, std::string> getLedgerTxs(RPC::Context& context, int ledgerSeq, uint256 startHash = beast::zero, bool include = false);
    void appendTxJson(const std::vector<std::shared_ptr<STTx>>& vecTxs, Json::Value& jvTxns, int ledgerSeq, int limit);

    static
        bool
        isHexTxID(std::string const& txid)
    {
        if (txid.size() != 64)
            return false;

        auto const ret = std::find_if(txid.begin(), txid.end(),
            [](std::string::value_type c)
        {
            return !std::isxdigit(c);
        });

        return (ret == txid.end());
    }

    static
        bool
        isValidated(RPC::Context& context, std::uint32_t seq, uint256 const& hash)
    {
        if (!context.ledgerMaster.haveLedger(seq))
            return false;

        if (seq > context.ledgerMaster.getValidatedLedger()->info().seq)
            return false;

        return context.ledgerMaster.getHashBySeq(seq) == hash;
    }

    bool
        getMetaHex(Ledger const& ledger,
            uint256 const& transID, std::string& hex)
    {
        SHAMapTreeNode::TNType type;
        auto const item =
            ledger.txMap().peekItem(transID, type);

        if (!item)
            return false;

        if (type != SHAMapTreeNode::tnTRANSACTION_MD)
            return false;

        SerialIter it(item->slice());
        it.getVL(); // skip transaction
        hex = strHex(makeSlice(it.getVL()));
        return true;
    }

    bool doTxChain(TxType txType, const RPC::Context& context, Json::Value& retJson)
    {
        if (!STTx::checkChainsqlTableType(txType) && !STTx::checkChainsqlContractType(txType))  return false;

        auto const txid = context.params[jss::transaction].asString();

        Json::Value jsonMetaChain,jsonContractChain,jsonTableChain;


        std::string sql = "SELECT TxSeq, TransType, Owner, Name FROM TraceTransactions WHERE TransID='";
        sql.append(txid);    sql.append("';");

        boost::optional<std::uint64_t> txSeq;
        boost::optional<std::string> previousTxid, nextTxid, ownerRead, nameRead, typeRead;
        {
            auto db = context.app.getTxnDB().checkoutDb();

            soci::statement st = (db->prepare << sql,
                soci::into(txSeq),
                soci::into(typeRead),
                soci::into(ownerRead),
                soci::into(nameRead));
            st.execute();

            std::string sSqlPrefix = "SELECT TransID FROM TraceTransactions WHERE TxSeq ";            

            while (st.fetch())
            {
                std::string sSqlSufix = boost::str(boost::format("'%lld' AND Owner = '%s' AND Name = '%s' order by TxSeq ")   \
                    % beast::lexicalCastThrow <std::string>(*txSeq)   \
                    % *ownerRead % *nameRead);
                std::string sSqlPrevious = sSqlPrefix + "<" + sSqlSufix + "desc limit 1";
                std::string sSqlNext = sSqlPrefix + ">" + sSqlSufix + "asc limit 1";

                Json::Value jsonTableItem;

                // get previous hash
                *db << sSqlPrevious, soci::into(previousTxid);

                if (isChainsqlContractType(*typeRead))
                {
                    jsonContractChain[jss::PreviousHash] = db->got_data() ? *previousTxid : "";
                }
                else
                {
                    jsonTableItem[jss::PreviousHash] = db->got_data() ? *previousTxid : "";
                }

                //get next hash
                *db << sSqlNext, soci::into(nextTxid);
                if (isChainsqlContractType(*typeRead))
                {
                    jsonContractChain[jss::NextHash] = db->got_data() ? *nextTxid : "";
                }
                else
                {
                    jsonTableItem[jss::NextHash] = db->got_data() ? *nextTxid : "";
                }

                if (!isChainsqlContractType(*typeRead))
                {
                    jsonTableItem[jss::NameInDB] = *nameRead;
                    jsonTableChain.append(jsonTableItem);                    
                }
            }
            if(jsonTableChain.size()>0)      jsonMetaChain[jss::TableChain]    = jsonTableChain;
            if(!jsonContractChain.isNull())  jsonMetaChain[jss::ContractChain] = jsonContractChain;

        }
        retJson[jss::metaChain] = jsonMetaChain;
        return true;
    }

Json::Value doTx (RPC::Context& context)
{
    if (!context.params.isMember (jss::transaction))
        return rpcError (rpcINVALID_PARAMS);

    bool binary = context.params.isMember (jss::binary)
            && context.params[jss::binary].asBool ();

    auto const txid  = context.params[jss::transaction].asString ();

    if (!isHexTxID (txid))
        return rpcError (rpcNOT_IMPL);

    auto txn = context.app.getMasterTransaction ().fetch (
        from_hex_text<uint256>(txid), true);

    if (!txn)
        return rpcError (rpcTXN_NOT_FOUND);

    Json::Value ret = txn->getJson (1, binary);

    if (txn->getLedger () == 0)
        return ret;

    if (auto lgr = context.ledgerMaster.getLedgerBySeq (txn->getLedger ()))
    {
        bool okay = false;

        if (binary)
        {
            std::string meta;

            if (getMetaHex (*lgr, txn->getID (), meta))
            {
                ret[jss::meta] = meta;
                okay = true;
            }
        }
        else
        {
            auto rawMeta = lgr->txRead (txn->getID()).second;
            if (rawMeta)
            {
                auto txMeta = std::make_shared<TxMeta> (txn->getID (),
                    lgr->seq (), *rawMeta, context.app.journal ("TxMeta"));
                okay = true;
                auto meta = txMeta->getJson (0);
                addPaymentDeliveredAmount (meta, context, txn, txMeta);
                ret[jss::meta] = meta;
            }
        }

        if (okay)
            ret[jss::validated] = isValidated (
                context, lgr->info().seq, lgr->info().hash);
    }       
    
    doTxChain(txn->getSTransaction()->getTxnType(), context, ret);

    return ret;
}

Json::Value doTxCount(RPC::Context& context)
{
	Json::Value ret(Json::objectValue);
	ret["chainsql"] = context.app.getMasterTransaction().getTxCount(true);
	ret["all"] = context.app.getMasterTransaction().getTxCount(false);

	return ret;
}

Json::Value doGetCrossChainTx(RPC::Context& context) 
{
	if (!context.params.isMember(jss::transaction_hash))
		return rpcError(rpcINVALID_PARAMS);

	auto const txid = context.params[jss::transaction_hash].asString();
	int ledgerIndex = 1;
	uint256 txHash = beast::zero;
	int limit = 1;
	bool bInclusive = true;

	try {
		if (isHexTxID(txid))
		{
			txHash = from_hex_text<uint256>(txid);
		}
		else if (txid.size() > 0 && txid.size() < 32)
		{
			ledgerIndex = std::stoi(txid);
		}
		else if (txid != "")
		{
			return rpcError(rpcNOT_IMPL);
		}

		if (context.params.isMember(jss::limit))
		{
			limit = std::max(context.params[jss::limit].asInt(), limit);
		}
		if (context.params.isMember(jss::inclusive))
		{
			bInclusive = context.params[jss::inclusive].asBool();
		}

		int maxSeq = context.ledgerMaster.getValidatedLedger()->info().seq;

		Json::Value ret(Json::objectValue);
		ret[jss::transaction_hash] = txid;
		ret[jss::limit] = limit;
		ret[jss::validated_ledger] = maxSeq;

		Json::Value& jvTxns = (ret[jss::transactions] = Json::arrayValue);

		int startLedger = ledgerIndex;
		int leftCount = limit;

		if (txHash != beast::zero)
		{
			auto txn = context.app.getMasterTransaction().fetch(txHash, true);
			if (!txn)
				return rpcError(rpcTXN_NOT_FOUND);

			auto txPair = getLedgerTxs(context, txn->getLedger(), txHash, bInclusive);
			if (txPair.second != "")
			{
				Json::Value json(Json::objectValue);
				json[jss::error_message] = txPair.second;
				return json;
			}

			appendTxJson(txPair.first, jvTxns,txn->getLedger(), leftCount);
			leftCount -= txPair.first.size();
			if (leftCount <= 0)
				return ret;

			startLedger = txn->getLedger() + 1;
		}

		//i should traverse from mCompleteLedgers
		for (int i = startLedger; i <= maxSeq; i++)
		{
			if (!context.app.getLedgerMaster().haveLedger(i))
				continue;

			auto txPair = getLedgerTxs(context, i);
			if (txPair.second != "")
			{
				Json::Value json(Json::objectValue);
				json[jss::error_message] = txPair.second;
				return json;
			}
			appendTxJson(txPair.first, jvTxns,i, leftCount);
			leftCount -= txPair.first.size();
			if (leftCount <= 0)
				return ret;
		}

		//auto end = std::chrono::system_clock::now();
		//using duration_type = std::chrono::duration<std::chrono::microseconds>;
		//auto duration2 = (end - start).count() * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
		//JLOG(debugLog().fatal()) << "---getCrossChainTx cost time:"<< duration2 <<" ledgerSeqs from "<<startLedger<<" to "<< maxSeq;
		//std::cerr << "---getCrossChainTx cost time:" << duration2 << " ledgerSeqs from " << startLedger << " to " << maxSeq << std::endl;

		return ret;
	}
	catch (std::exception const&)
	{
		return rpcError(rpcINTERNAL);
	}
}

void appendTxJson(const std::vector<std::shared_ptr<STTx>>& vecTxs, Json::Value& jvTxns,int ledgerSeq,int limit)
{
	int count = std::min(limit,(int)vecTxs.size());
	for (int i = 0; i < count; i++)
	{
		Json::Value& jvObj = jvTxns.append(Json::objectValue);
		auto pSTTx = vecTxs[i];
		jvObj[jss::tx] = pSTTx->getJson(1);
		jvObj[jss::tx][jss::ledger_index] = ledgerSeq;
	}
}

//get txs from a ledger
std::pair<std::vector<std::shared_ptr<STTx>>,std::string> getLedgerTxs(RPC::Context& context,int ledgerSeq,uint256 startHash,bool include)
{
	std::vector<std::shared_ptr<STTx>> vecTxs;
	auto ledger = context.ledgerMaster.getLedgerBySeq(ledgerSeq);
	if (ledger == NULL)
	{        
		std::string error = "Get ledger " + to_string(ledgerSeq) + std::string(" failed");
		return std::make_pair(vecTxs, error);
	}
	bool bFound = false;
	for (auto const& item : ledger->txMap())
	{
		try
		{
			auto blob = SerialIter{ item.data(), item.size() }.getVL();
			std::shared_ptr<STTx> pSTTX = std::make_shared<STTx>(SerialIter{ blob.data(), blob.size() });
			if (pSTTX->isChainSqlTableType())
			{
				if (startHash != beast::zero && !bFound)
				{
					if (pSTTX->getTransactionID() == startHash)
					{
						bFound = true;
						if(!include)
							continue;
					}
					else
						continue;						
				}
				vecTxs.push_back(pSTTX);
			}
		}
		catch (std::exception const&)
		{
			//JLOG(journal_.warn()) << "Txn " << item.key() << " throws";
		}
	}
	return std::make_pair(vecTxs, "");
}


} // ripple
