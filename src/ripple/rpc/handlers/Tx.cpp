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


#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/basics/ToString.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/DeliveredAmount.h>
#include <ripple/rpc/GRPCHandlers.h>
#include <ripple/rpc/impl/GRPCHelpers.h>
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

static bool
isHexTxID(std::string const& txid)
{
    if (txid.size() != 64)
        return false;

    auto const ret =
        std::find_if(txid.begin(), txid.end(), [](std::string::value_type c) {
            return !std::isxdigit(static_cast<unsigned char>(c));
        });

    return (ret == txid.end());
}

static bool
isValidated(LedgerMaster& ledgerMaster, std::uint32_t seq, uint256 const& hash)
{
    if (!ledgerMaster.haveLedger(seq))
        return false;

    if (seq > ledgerMaster.getValidatedLedger()->info().seq)
        return false;

    return ledgerMaster.getHashBySeq(seq) == hash;
}

bool
getMetaHex(Ledger const& ledger, uint256 const& transID, std::string& hex)
{
    SHAMapTreeNode::TNType type;
    auto const item = ledger.txMap().peekItem(transID, type);

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

enum class SearchedAll { no, yes, unknown };

struct TxResult
{
    Transaction::pointer txn;
    std::variant<std::shared_ptr<TxMeta>, Blob> meta;
    bool validated = false;
    SearchedAll searchedAll;
};

struct TxArgs
{
    uint256 hash;
    bool binary = false;
    std::optional<std::pair<uint32_t, uint32_t>> ledgerRange;
};

std::pair<TxResult, RPC::Status>
doTxHelp(RPC::Context& context, TxArgs const& args)
{
    TxResult result;

    ClosedInterval<uint32_t> range;

    if (args.ledgerRange)
    {
        constexpr uint16_t MAX_RANGE = 1000;

        if (args.ledgerRange->second < args.ledgerRange->first)
            return {result, rpcINVALID_LGR_RANGE};

        if (args.ledgerRange->second - args.ledgerRange->first > MAX_RANGE)
            return {result, rpcEXCESSIVE_LGR_RANGE};

        range = ClosedInterval<uint32_t>(
            args.ledgerRange->first, args.ledgerRange->second);
    }

    std::shared_ptr<Transaction> txn;
    auto ec{rpcSUCCESS};

    result.searchedAll = SearchedAll::unknown;
    if (args.ledgerRange)
    {
        boost::variant<std::shared_ptr<Transaction>, bool> v =
            context.app.getMasterTransaction().fetch(args.hash, range, ec);

        if (v.which() == 1)
        {
            result.searchedAll =
                boost::get<bool>(v) ? SearchedAll::yes : SearchedAll::no;
            return {result, rpcTXN_NOT_FOUND};
        }
        else
        {
            txn = boost::get<std::shared_ptr<Transaction>>(v);
        }
    }
    else
    {
        txn = context.app.getMasterTransaction().fetch(args.hash, ec);
    }

    if (ec == rpcDB_DESERIALIZATION)
    {
        return {result, ec};
    }
    if (!txn)
    {
        return {result, rpcTXN_NOT_FOUND};
    }

    // populate transaction data
    result.txn = txn;
    if (txn->getLedger() == 0)
    {
        return {result, rpcSUCCESS};
    }

    std::shared_ptr<Ledger const> ledger =
        context.ledgerMaster.getLedgerBySeq(txn->getLedger());
    // get meta data
    if (ledger)
    {
        bool ok = false;
        if (args.binary)
        {
            SHAMapTreeNode::TNType type;
            auto const item = ledger->txMap().peekItem(txn->getID(), type);

            if (item && type == SHAMapTreeNode::tnTRANSACTION_MD)
            {
                ok = true;
                SerialIter it(item->slice());
                it.skip(it.getVLDataLength());  // skip transaction
                Blob blob = it.getVL();
                result.meta = std::move(blob);
            }
        }
        else
        {
            auto rawMeta = ledger->txRead(txn->getID()).second;
            if (rawMeta)
            {
                ok = true;
                result.meta = std::make_shared<TxMeta>(
                    txn->getID(), ledger->seq(), *rawMeta);
            }
        }
        if (ok)
        {
            result.validated = isValidated(
                context.ledgerMaster, ledger->info().seq, ledger->info().hash);
        }
    }

    return {result, rpcSUCCESS};
}

std::pair<org::zxcl::rpc::v1::GetTransactionResponse, grpc::Status>
populateProtoResponse(
    std::pair<TxResult, RPC::Status> const& res,
    TxArgs const& args,
    RPC::GRPCContext<org::zxcl::rpc::v1::GetTransactionRequest> const& context)
{
    org::zxcl::rpc::v1::GetTransactionResponse response;
    grpc::Status status = grpc::Status::OK;
    RPC::Status const& error = res.second;
    TxResult const& result = res.first;
    // handle errors
    if (error.toErrorCode() != rpcSUCCESS)
    {
        if (error.toErrorCode() == rpcTXN_NOT_FOUND &&
            result.searchedAll != SearchedAll::unknown)
        {
            status = {
                grpc::StatusCode::NOT_FOUND,
                "txn not found. searched_all = " +
                    to_string(
                        (result.searchedAll == SearchedAll::yes ? "true"
                                                                : "false"))};
        }
        else
        {
            if (error.toErrorCode() == rpcTXN_NOT_FOUND)
                status = {grpc::StatusCode::NOT_FOUND, "txn not found"};
            else
                status = {grpc::StatusCode::INTERNAL, error.message()};
        }
    }
    // no errors
    else if (result.txn)
    {
        auto& txn = result.txn;

        std::shared_ptr<STTx const> stTxn = txn->getSTransaction();
        if (args.binary)
        {
            Serializer s = stTxn->getSerializer();
            response.set_transaction_binary(s.data(), s.size());
        }
        else
        {
            RPC::convert(*response.mutable_transaction(), stTxn);
        }

        response.set_hash(context.params.hash());

        auto ledgerIndex = txn->getLedger();
        response.set_ledger_index(ledgerIndex);
        if (ledgerIndex)
        {
            auto ct =
                context.app.getLedgerMaster().getCloseTimeBySeq(ledgerIndex);
            if (ct)
                response.mutable_date()->set_value(
                    ct->time_since_epoch().count());
        }

        RPC::convert(
            *response.mutable_meta()->mutable_transaction_result(),
            txn->getResult());
        response.mutable_meta()->mutable_transaction_result()->set_result(
            transToken(txn->getResult()));

        // populate binary metadata
        if (auto blob = std::get_if<Blob>(&result.meta))
        {
            assert(args.binary);
            Slice slice = makeSlice(*blob);
            response.set_meta_binary(slice.data(), slice.size());
        }
        // populate meta data
        else if (auto m = std::get_if<std::shared_ptr<TxMeta>>(&result.meta))
        {
            auto& meta = *m;
            if (meta)
            {
                RPC::convert(*response.mutable_meta(), meta);
                auto amt =
                    getDeliveredAmount(context, stTxn, *meta, txn->getLedger());
                if (amt)
                {
                    RPC::convert(
                        *response.mutable_meta()->mutable_delivered_amount(),
                        *amt);
                }
            }
        }
        response.set_validated(result.validated);
    }
    return {response, status};
}

Json::Value
populateJsonResponse(
    std::pair<TxResult, RPC::Status> const& res,
    TxArgs const& args,
    RPC::JsonContext const& context)
{
    Json::Value response;
    RPC::Status const& error = res.second;
    TxResult const& result = res.first;
    // handle errors
    if (error.toErrorCode() != rpcSUCCESS)
    {
        if (error.toErrorCode() == rpcTXN_NOT_FOUND &&
            result.searchedAll != SearchedAll::unknown)
        {
            response = Json::Value(Json::objectValue);
            response[jss::searched_all] =
                (result.searchedAll == SearchedAll::yes);
            error.inject(response);
        }
        else
        {
            error.inject(response);
        }
    }
    // no errors
    else if (result.txn)
    {
        response = result.txn->getJson(JsonOptions::include_date, args.binary);

        // populate binary metadata
        if (auto blob = std::get_if<Blob>(&result.meta))
        {
            assert(args.binary);
            response[jss::meta] = strHex(makeSlice(*blob));
        }
        // populate meta data
        else if (auto m = std::get_if<std::shared_ptr<TxMeta>>(&result.meta))
        {
            auto& meta = *m;
            if (meta)
            {
                response[jss::meta] = meta->getJson(JsonOptions::none);
                insertDeliveredAmount(
                    response[jss::meta], context, result.txn, *meta);
            }
        }
        response[jss::validated] = result.validated;
    }

    doTxChain(txn->getSTransaction()->getTxnType(), context, response);
    
    return response;
}

Json::Value
doTxJson(RPC::JsonContext& context)
{
    // Deserialize and validate JSON arguments

    if (!context.params.isMember(jss::transaction))
        return rpcError(rpcINVALID_PARAMS);

    std::string txHash = context.params[jss::transaction].asString();
    if (!isHexTxID(txHash))
        return rpcError(rpcNOT_IMPL);

    TxArgs args;
    args.hash = from_hex_text<uint256>(txHash);

    args.binary = context.params.isMember(jss::binary) &&
        context.params[jss::binary].asBool();

    if (context.params.isMember(jss::min_ledger) &&
        context.params.isMember(jss::max_ledger))
    {
        try
        {
            args.ledgerRange = std::make_pair(
                context.params[jss::min_ledger].asUInt(),
                context.params[jss::max_ledger].asUInt());
        }
        catch (...)
        {
            // One of the calls to `asUInt ()` failed.
            return rpcError(rpcINVALID_LGR_RANGE);
        }
    }

    std::pair<TxResult, RPC::Status> res = doTxHelp(context, args);
    return populateJsonResponse(res, args, context);
}

std::pair<org::zxcl::rpc::v1::GetTransactionResponse, grpc::Status>
doTxGrpc(RPC::GRPCContext<org::zxcl::rpc::v1::GetTransactionRequest>& context)
{
    // return values
    org::zxcl::rpc::v1::GetTransactionResponse response;
    grpc::Status status = grpc::Status::OK;

    // input
    org::zxcl::rpc::v1::GetTransactionRequest& request = context.params;

    TxArgs args;

    std::string const& hashBytes = request.hash();
    args.hash = uint256::fromVoid(hashBytes.data());
    if (args.hash.size() != hashBytes.size())
    {
        grpc::Status errorStatus{
            grpc::StatusCode::INVALID_ARGUMENT, "ledger hash malformed"};
        return {response, errorStatus};
    }

    args.binary = request.binary();

    if (request.ledger_range().ledger_index_min() != 0 &&
        request.ledger_range().ledger_index_max() != 0)
    {
        args.ledgerRange = std::make_pair(
            request.ledger_range().ledger_index_min(),
            request.ledger_range().ledger_index_max());
    }

    std::pair<TxResult, RPC::Status> res = doTxHelp(context, args);
    return populateProtoResponse(res, args, context);
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
		jvObj[jss::tx] = pSTTx->getJson();
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
