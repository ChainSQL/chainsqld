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
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/basics/ToString.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/DeliveredAmount.h>
#include <ripple/rpc/GRPCHandlers.h>
#include <ripple/rpc/impl/GRPCHelpers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <boost/optional/optional_io.hpp>
#include <chrono>
#include <peersafe/app/util/Common.h>
#include <peersafe/crypto/hashBaseObj.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <peersafe/app/misc/TxPool.h>

namespace ripple {

// {
//   transaction: <hex>
// }
std::pair<std::vector<std::shared_ptr<STTx>>, std::string>
getLedgerTxs(
    RPC::JsonContext& context,
    int ledgerSeq,
    uint256 startHash = beast::zero,
    bool include = false);
void
appendTxJson(
    const std::vector<std::shared_ptr<STTx>>& vecTxs,
    Json::Value& jvTxns,
    int ledgerSeq,
    int limit);

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
    it.getVL();  // skip transaction
    hex = strHex(makeSlice(it.getVL()));
    return true;
}
bool
getRawMetaHex(
    Ledger const& ledger,
    uint256 const& transID,
    std::string& rawHex,
    std::string& metaHex)
{
    SHAMapTreeNode::TNType type;
    auto const item = ledger.txMap().peekItem(transID, type);

    if (!item)
        return false;

    if (type != SHAMapTreeNode::tnTRANSACTION_MD)
        return false;

    SerialIter it(item->slice());
    rawHex = strHex(makeSlice(it.getVL()));
    metaHex = strHex(makeSlice(it.getVL()));
    return true;
}

bool
getRawMeta(Ledger const& ledger, uint256 const& transID, Blob& raw, Blob& meta)
{
    SHAMapTreeNode::TNType type;
    auto const item = ledger.txMap().peekItem(transID, type);

    if (!item)
        return false;

    if (type != SHAMapTreeNode::tnTRANSACTION_MD)
        return false;

    SerialIter it(item->slice());
    raw = it.getVL();
    meta = it.getVL();
    return true;
}

bool
doTxChain(TxType txType, const RPC::JsonContext& context, Json::Value& retJson)
{
    if (!STTx::checkChainsqlTableType(txType) &&
        !STTx::checkChainsqlContractType(txType))
        return false;

    auto const txid = context.params[jss::transaction].asString();

    Json::Value jsonMetaChain, jsonContractChain, jsonTableChain;

    std::string sql =
        "SELECT TxSeq, TransType, Owner, Name FROM TraceTransactions WHERE "
        "TransID='";
    sql.append(txid);
    sql.append("';");

    boost::optional<std::uint64_t> txSeq;
    boost::optional<std::string> previousTxid, nextTxid, ownerRead, nameRead,
        typeRead;
    {
        auto db = context.app.getTxnDB().checkoutDbRead();

        soci::statement st =
            (db->prepare << sql,
             soci::into(txSeq),
             soci::into(typeRead),
             soci::into(ownerRead),
             soci::into(nameRead));
        st.execute();

        std::string sSqlPrefix =
            "SELECT TransID FROM TraceTransactions WHERE TxSeq ";

        while (st.fetch())
        {
            std::string sSqlSufix = boost::str(
                boost::format(
                    "'%lld' AND Owner = '%s' AND Name = '%s' order by TxSeq ") %
                beast::lexicalCastThrow<std::string>(*txSeq) % *ownerRead %
                *nameRead);
            std::string sSqlPrevious =
                sSqlPrefix + "<" + sSqlSufix + "desc limit 1";
            std::string sSqlNext = sSqlPrefix + ">" + sSqlSufix + "asc limit 1";

            Json::Value jsonTableItem;

            // get previous hash
            *db << sSqlPrevious, soci::into(previousTxid);

            if (isChainsqlContractType(*typeRead))
            {
                jsonContractChain[jss::PreviousHash] =
                    db->got_data() ? *previousTxid : "";
            }
            else
            {
                jsonTableItem[jss::PreviousHash] =
                    db->got_data() ? *previousTxid : "";
            }

            // get next hash
            *db << sSqlNext, soci::into(nextTxid);
            if (isChainsqlContractType(*typeRead))
            {
                jsonContractChain[jss::NextHash] =
                    db->got_data() ? *nextTxid : "";
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
        if (jsonTableChain.size() > 0)
            jsonMetaChain[jss::TableChain] = jsonTableChain;
        if (!jsonContractChain.isNull())
            jsonMetaChain[jss::ContractChain] = jsonContractChain;
    }
    retJson[jss::meta_chain] = jsonMetaChain;
    return true;
}

enum class SearchedAll { no, yes, unknown };

struct TxResult
{
    Transaction::pointer txn;
    std::variant<std::shared_ptr<TxMeta>, Blob> meta;
    std::shared_ptr<Ledger const> ledger;
    bool validated = false;
    SearchedAll searchedAll;
};

struct TxArgs
{
    uint256 hash;
    bool binary = false;
    bool metaData = true;
    bool metaChain = true;
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

    result.searchedAll = SearchedAll::unknown;
    if (args.ledgerRange)
    {
        boost::variant<std::shared_ptr<Transaction>, bool> v =
            context.app.getMasterTransaction().fetch(args.hash, range);

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
        txn = context.app.getMasterTransaction().fetch(args.hash);
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

    // get meta data
    if (args.metaData)
    {
        if (args.binary)
        {
            result.meta = txn->getMeta();
        }
        else
        {
            result.meta = std::make_shared<TxMeta>(
                txn->getID(), txn->getLedger(), txn->getMeta());
        }
    }    

    std::shared_ptr<Ledger const> ledger =
        context.ledgerMaster.getLedgerBySeq(txn->getLedger());
    if (ledger)
    {
        result.ledger = ledger;
        result.validated = isValidated(
            context.ledgerMaster, ledger->info().seq, ledger->info().hash);
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
        if (args.metaData)
        {
            if (auto blob = std::get_if<Blob>(&result.meta))
            {
                assert(args.binary);
                response[jss::meta] = strHex(makeSlice(*blob));
            }
            // populate meta data
            else if (
                auto m = std::get_if<std::shared_ptr<TxMeta>>(&result.meta))
            {
                auto& meta = *m;
                if (meta)
                {
                    response[jss::meta] = meta->getJson(JsonOptions::none);
                    insertDeliveredAmount(
                        response[jss::meta], context, result.txn, *meta);
                }
            }
        }
        response[jss::tx_status] = to_string(result.txn->getStatus());
        response[jss::validated] = result.validated;
    }

    if (result.txn && args.metaChain && context.app.config().USE_TRACE_TABLE)
        doTxChain(
            result.txn->getSTransaction()->getTxnType(), context, response);

    return response;
}

Json::Value
doTxJson(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return rpcError(rpcNOT_ENABLED);
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

    if (context.params.isMember(jss::meta) &&
        !context.params[jss::meta].asBool())
    {
        args.metaData = false;
    }

    if (context.params.isMember(jss::meta_chain) &&
        !context.params[jss::meta_chain].asBool())
    {
        args.metaChain = false;
    }
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

std::pair<std::shared_ptr<STTx const>, Json::Value>
jsonToSTTx(Json::Value& params, CommonKey::HashType hashType = CommonKey::chainHashTypeG)
{
    std::shared_ptr<STTx const> sttx;

    if (params.isMember(jss::tx_json))
    {
        if (!params[jss::tx_json].isObject())
        {
            return std::make_pair(
                nullptr, RPC::make_param_error("Object tx_json is malformed."));
        }

        params[jss::tx_json].removeMember(jss::date);
        params[jss::tx_json].removeMember(jss::inLedger);
        params[jss::tx_json].removeMember(jss::ledger_index);
        try
        {
            STParsedJSONObject parsed(
                std::string(jss::tx_json), params[jss::tx_json]);
            if (parsed.object == boost::none)
            {
                Json::Value err;
                err[jss::error] = parsed.error[jss::error];
                err[jss::error_code] = parsed.error[jss::error_code];
                err[jss::error_message] = parsed.error[jss::error_message];
                return std::make_pair(nullptr, err);
            }

            sttx = std::make_shared<STTx>(
                std::move(parsed.object.get()), hashType);
        }
        catch (STObject::FieldErr& err)
        {
            return std::make_pair(
                nullptr, RPC::make_error(rpcINVALID_PARAMS, err.what()));
        }
        catch (std::exception&)
        {
            return std::make_pair(
                nullptr,
                RPC::make_error(
                    rpcINTERNAL,
                    "Exception occurred constructing serialized transaction"));
        }
    }
    else
    {
        auto blob = strUnHex(params[jss::tx].asString());
        if (!blob || !blob->size())
            return std::make_pair(nullptr, rpcError(rpcINVALID_PARAMS));
        if (blob->size() > RPC::Tuning::max_txn_size)
            return std::make_pair(
                nullptr, rpcError(rpcTXN_BIGGER_THAN_MAXSIZE));

        SerialIter sitTrans(makeSlice(*blob));

        try
        {
            sttx = std::make_shared<STTx const>(std::ref(sitTrans), hashType);
        }
        catch (std::exception& e)
        {
            Json::Value err;
            err[jss::error] = "invalidTransaction";
            err[jss::error_code] = rpcINVALID_TRANSACTION;
            err[jss::error_message] = e.what();
            return std::make_pair(nullptr, err);
        }
    }

    return std::make_pair(sttx, Json::objectValue);
}

std::pair<std::shared_ptr<TxMeta>, Json::Value>
jsonToTxMeta(Json::Value& params, uint256 txHash, std::uint32_t ledgerSeq)
{
    if (!params[jss::meta].isObject() && !params[jss::meta].isString())
        return std::make_pair(
            nullptr, RPC::make_param_error("Element meta is malformed."));

    std::shared_ptr<TxMeta> meta;
    if (params[jss::meta].isObject())
    {
        params[jss::meta].removeMember(jss::delivered_amount);
        try
        {
            STParsedJSONObject parsed(
                std::string(jss::meta), params[jss::meta]);
            if (parsed.object == boost::none)
            {
                Json::Value err;
                err[jss::error] = parsed.error[jss::error];
                err[jss::error_code] = parsed.error[jss::error_code];
                err[jss::error_message] = parsed.error[jss::error_message];
                return std::make_pair(nullptr, err);
            }

            meta = std::make_shared<TxMeta>(
                txHash, ledgerSeq, std::move(parsed.object.get()));
        }
        catch (STObject::FieldErr& err)
        {
            return std::make_pair(
                nullptr, RPC::make_error(rpcINVALID_PARAMS, err.what()));
        }
        catch (std::exception&)
        {
            return std::make_pair(
                nullptr,
                RPC::make_error(
                    rpcINTERNAL,
                    "Exception occurred constructing serialized TxMeta"));
        }
    }
    else
    {
        auto blob = strUnHex(params[jss::meta].asString());
        if (!blob || !blob->size())
            return std::make_pair(
                nullptr, RPC::make_param_error("Element meta is malformed."));

        try
        {
            meta = std::make_shared<TxMeta>(txHash, ledgerSeq, *blob);
        }
        catch (...)
        {
            return std::make_pair(nullptr, RPC::invalid_field_error(jss::meta));
        }
    }

    return std::make_pair(meta, Json::objectValue);
}

bool
verifyProof(
    uint256 const& nodeHash,
    Blob const& proofBlob,
    uint256 const& rootHash,
    CommonKey::HashType hashType)
{
    std::string s;
    s.assign(proofBlob.begin(), proofBlob.end());

    protocol::TMSHAMapProof proof;

    if (!proof.ParseFromString(s))
        return false;

    if (proof.rootid().size() != uint256::size())
        return false;

    uint256 rootID;
    memcpy(rootID.begin(), proof.rootid().data(), 32);
    if (rootID != rootHash)
        return false;

    uint256 child = nodeHash;

    if (proof.level_size() == 0)
        return child == rootID;

    for (int rdepth = 0; rdepth < proof.level_size(); ++rdepth)
    {
        auto level = proof.level(rdepth);

        auto branchCount = [&]() {
            int count = 0;
            for (int i = 0; i < 16; ++i)
            {
                if (level.branch() & (1 << i))
                    count++;
            }
            return count;
        }();
        if (!branchCount || branchCount != level.nodeid_size())
            return false;

        bool found = false;
        std::array<uint256, 16> hashes = {beast::zero};

        for (int branch = 0, index = 0; branch < 16; ++branch)
        {
            if (level.branch() & (1 << branch))
            {
                if (level.nodeid(index).size() != 32)
                    return false;

                uint256 nodeID = beast::zero;
                memcpy(nodeID.begin(), level.nodeid(index).data(), 32);

                if (nodeID == child)
                    found = true;

                hashes[branch] = nodeID;
                index++;
            }
        }

        if (!found)
            return false;

        std::unique_ptr<hashBase> hasher = hashBaseObj::getHasher(hashType);
        using beast::hash_append;
        hash_append(*hasher, HashPrefix::innerNode);
        for (auto const& hh : hashes)
            hash_append(*hasher, hh);
        child = static_cast<typename sha512_half_hasher::result_type>(*hasher);
    }

    return child == rootID;
}

Json::Value
doTxMerkleProof(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return rpcError(rpcNOT_ENABLED);
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

    std::pair<TxResult, RPC::Status> res = doTxHelp(context, args);

    Json::Value response;
    RPC::Status const& error = res.second;
    TxResult const& result = res.first;
    // handle errors
    if (error.toErrorCode() != rpcSUCCESS)
    {
        return rpcError(error.toErrorCode());
    }

    assert(result.txn);

    if (!result.ledger || !result.validated)
    {
        return rpcError(rpcTXN_NOT_VALIDATED);
    }

    // no errors
    if (args.binary)
        response = result.txn->getSTransaction()->getJson(
            JsonOptions::none, args.binary);
    else
        response[jss::tx_json] =
            result.txn->getSTransaction()->getJson(JsonOptions::none);

    if (auto blob = std::get_if<Blob>(&result.meta))
    {
        // populate binary metadata
        assert(args.binary);
        response[jss::meta] = strHex(makeSlice(*blob));
    }
    else if (auto m = std::get_if<std::shared_ptr<TxMeta>>(&result.meta))
    {
        // populate meta data
        auto& meta = *m;
        if (meta)
        {
            response[jss::meta] = meta->getJson(JsonOptions::none);
        }
    }

    try
    {
        auto const proof = result.ledger->txMap().genNodeProof(args.hash);
        if (proof.first == beast::zero || proof.second.size() <= 0)
        {
            return rpcError(rpcTXN_NOT_FOUND);
        }

        response[jss::tx_node_hash] = to_string(proof.first);
        response[jss::proof] = strHex(proof.second);
    }
    catch (...)
    {
        return rpcError(rpcINTERNAL);
    }

    response[jss::ledger_hash] = to_string(result.ledger->info().hash);

    response[jss::schema_id] = to_string(context.app.schemaId());

    response[jss::crypto_alg] = CommonKey::getHashTypeStr();

    return response;
}

Json::Value
doTxMerkleVerifyFromCommandline(RPC::JsonContext& context)
{
    if (!context.params.isMember(jss::transaction_hash))
        return RPC::missing_field_error(jss::transaction_hash);

    if (!context.params.isMember(jss::proof))
        return RPC::missing_field_error(jss::proof);

    std::string txHash = context.params[jss::transaction_hash].asString();
    if (!isHexTxID(txHash))
        return rpcError(rpcNOT_IMPL);

    TxArgs args;
    args.hash = from_hex_text<uint256>(txHash);

    args.binary = context.params.isMember(jss::binary) &&
        context.params[jss::binary].asBool();

    std::pair<TxResult, RPC::Status> res = doTxHelp(context, args);

    RPC::Status const& error = res.second;
    TxResult const& result = res.first;
    // handle errors
    if (error.toErrorCode() != rpcSUCCESS)
    {
        return rpcError(error.toErrorCode());
    }

    assert(result.txn);

    if (!result.ledger || !result.validated)
    {
        return rpcError(rpcTXN_NOT_VALIDATED);
    }

    // Get SHAMapItem
    auto item = result.ledger->txMap().peekItem(args.hash);
    if (!item)
    {
        return rpcError(rpcTXN_NOT_FOUND);
    }

    // Make SHAMapTreeNode
    std::shared_ptr<SHAMapTreeNode> node = std::make_shared<SHAMapTreeNode>(
        std::move(item), SHAMapTreeNode::tnTRANSACTION_MD, 0);

    // Verify node merkle proof
    if (!context.params[jss::proof].isString())
        return RPC::make_param_error(
            "Element proof is malformed, must be a string.");
    auto proof = strUnHex(context.params[jss::proof].asString());
    if (!proof || !proof->size())
        return RPC::make_param_error(
            "Element proof is malformed, must be a hex string.");

    if (!verifyProof(
            node->getNodeHash().as_uint256(),
            *proof,
            result.ledger->info().txHash,
            CommonKey::chainHashTypeG))
    {
        return RPC::make_error(
            rpcBAD_PROOF, "Tx node merkle proof verify failed.");
    }

    return Json::objectValue;
}

Json::Value
doTxMerkleVerifyWithTx(
    RPC::JsonContext& context,
    LedgerInfo const& info,
    Blob const& proof,
    CommonKey::HashType hashType)
{
    // Prase params.tx_json or params.tx to STTx
    auto sttx = jsonToSTTx(context.params, hashType);
    if (!sttx.first)
        return sttx.second;

    // Verify step-2: optional verify tx hash
    if ((context.params.isMember(jss::tx_json) &&
         context.params[jss::tx_json].isMember(jss::hash)) ||
        context.params.isMember(jss::hash))
    {
        std::string hash = context.params.isMember(jss::hash)
            ? context.params[jss::hash].asString()
            : context.params[jss::tx_json][jss::hash].asString();
        if (!isHexID(hash))
            return rpcError(rpcNOT_IMPL);

        if (sttx.first->getTransactionID() != from_hex_text<uint256>(hash))
        {
            return RPC::make_error(
                rpcTX_HASH_NOT_MATCH, "Tx json does not match tx hash.");
        }
    }

    // Prase params.meta to TxMeta
    auto meta =
        jsonToTxMeta(context.params, sttx.first->getTransactionID(), info.seq);
    if (!meta.first)
        return meta.second;

    // Make SHAMapItem
    auto const sTx = std::make_shared<Serializer>();
    sttx.first->add(*sTx);
    auto sMeta = std::make_shared<Serializer>();
    meta.first->addRaw(
        *sMeta, meta.first->getResultTER(), meta.first->getIndex());

    Serializer s(sTx->getDataLength() + sMeta->getDataLength() + 16);
    s.addVL(sTx->peekData());
    s.addVL(sMeta->peekData());
    auto item = std::make_shared<SHAMapItem const>(
        sttx.first->getTransactionID(), std::move(s));

    // Make SHAMapTreeNode
    std::shared_ptr<SHAMapTreeNode> node = std::make_shared<SHAMapTreeNode>(
        std::move(item), SHAMapTreeNode::tnTRANSACTION_MD, 0, hashType);

    // Verify step-3: optional verify tx node hash
    if (context.params.isMember(jss::tx_node_hash))
    {
        std::string hash = context.params[jss::tx_node_hash].asString();
        if (!isHexID(hash))
            return rpcError(rpcNOT_IMPL);

        if (from_hex_text<uint256>(hash) != node->getNodeHash().as_uint256())
        {
            return RPC::make_error(
                rpcTX_NODEHASH_NOT_MATCH,
                "Tx with meta does not match tx node hash.");
        }
    }

    // Verify step-4: verify node merkle proof
    if (!verifyProof(
            node->getNodeHash().as_uint256(), proof, info.txHash, hashType))
    {
        return RPC::make_error(
            rpcBAD_PROOF, "Tx node merkle proof verify failed.");
    }

    return Json::objectValue;
}

Json::Value
doTxMerkleVerifyWithNodeHash(
    RPC::JsonContext& context,
    LedgerInfo const& info,
    Blob const& proof,
    CommonKey::HashType hashType)
{
    if (!context.params.isMember(jss::tx_node_hash))
        return RPC::missing_field_error(jss::tx_node_hash);

    std::string hash = context.params[jss::tx_node_hash].asString();
    if (!isHexID(hash))
        return rpcError(rpcNOT_IMPL);

    // Verify step-2: verify node merkle proof
    if (!verifyProof(
            from_hex_text<uint256>(hash), proof, info.txHash, hashType))
    {
        return RPC::make_error(
            rpcBAD_PROOF, "Tx node merkle proof verify failed.");
    }

    return Json::objectValue;
}

Json::Value
doTxMerkleVerifyFromRPC(RPC::JsonContext& context)
{
    if (!context.params.isMember(jss::ledger))
        return RPC::missing_field_error(jss::ledger);

    if (!context.params.isMember(jss::proof))
        return RPC::missing_field_error(jss::proof);

    // Get hash type
    CommonKey::HashType hashType = CommonKey::chainHashTypeG;
    if (context.params.isMember(jss::crypto_alg))
    {
        if (!context.params[jss::crypto_alg].isString())
            return RPC::make_param_error(
                "Element crypto_alg is malformed, must be a string.");
        hashType = CommonKey::hashTypeFromString(
            context.params[jss::crypto_alg].asString());
        if (hashType == CommonKey::unknown)
            return RPC::make_param_error("Element crypto_alg is invalid.");
    }

    // Prase params.ledger to LedgerInfo
    LedgerInfo info;
    try
    {
        if (!fromJson(info, context.params[jss::ledger]))
        {
            return RPC::make_param_error("Object ledger is malformed.");
        }
    }
    catch (...)
    {
        return RPC::make_param_error("Object ledger is malformed.");
    }

    // Verify step-1: verify ledger info
    if (context.params[jss::ledger].isMember(jss::hash))
    {
        std::string hash = context.params[jss::ledger][jss::hash].asString();
        if (!isHexID(hash))
            return rpcError(rpcNOT_IMPL);
        info.hash = from_hex_text<uint256>(hash);

        uint256 ledgerHash;
        if (hashType == CommonKey::sha)
            ledgerHash = calculateLedgerHash<CommonKey::sha>(info);
        else
            ledgerHash = calculateLedgerHash<CommonKey::sm3>(info);

        if (ledgerHash != info.hash)
        {
            return RPC::make_error(
                rpcUNRELIABLE_LEDGER_HEADER,
                "Ledger info and ledger hash not match.");
        }
    }

    if (!context.params[jss::proof].isString())
        return RPC::make_param_error(
            "Element proof is malformed, must be a string.");
    auto proof = strUnHex(context.params[jss::proof].asString());
    if (!proof || !proof->size())
        return RPC::make_param_error(
            "Element proof is malformed, must be a hex string.");

    if ((context.params.isMember(jss::tx_json) ||
         context.params.isMember(jss::tx)) &&
        context.params.isMember(jss::meta))
        return doTxMerkleVerifyWithTx(context, info, *proof, hashType);
    else
        return doTxMerkleVerifyWithNodeHash(context, info, *proof, hashType);
}

Json::Value
doTxMerkleVerify(RPC::JsonContext& context)
{
    if (context.params.isMember(jss::transaction_hash) &&
        context.params.isMember(jss::proof))
    {
        return doTxMerkleVerifyFromCommandline(context);
    }

    return doTxMerkleVerifyFromRPC(context);
}

std::pair<org::zxcl::rpc::v1::GetTransactionResponse, grpc::Status>
doTxGrpc(RPC::GRPCContext<org::zxcl::rpc::v1::GetTransactionRequest>& context)
{
    if (!context.app.config().useTxTables())
    {
        return {
            {},
            {grpc::StatusCode::UNIMPLEMENTED, "Not enabled in configuration."}};
    }

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
        grpc::Status errorStatus{grpc::StatusCode::INVALID_ARGUMENT,
                                 "ledger hash malformed"};
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

Json::Value doTxResult(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
    {
        return rpcError(rpcNOT_ENABLED);
    }

	if (!context.params.isMember(jss::transaction))
		return rpcError(rpcINVALID_PARAMS);

	auto const txid = context.params[jss::transaction].asString();

	if (!isHexTxID(txid))
		return rpcError(rpcNOT_IMPL);
	auto txHash = from_hex_text<uint256>(txid);

	std::string sql = "SELECT LedgerSeq, TxResult FROM Transactions WHERE TransID='";
	sql.append(txid);    
	sql.append("';");

	Json::Value ret(Json::objectValue);
	ret[jss::tx_hash] = txid;
	boost::optional<std::uint32_t> LedgerSeq;
	boost::optional<std::string> TxResult;

	auto db = context.app.getTxnDB().checkoutDbRead();

	soci::statement st = (db->prepare << sql,
		soci::into(LedgerSeq),
		soci::into(TxResult));
	st.execute(); 
	if (st.fetch() && context.app.getLedgerMaster().haveLedger(*LedgerSeq))
	{
		ret[jss::ledger_index] = *LedgerSeq;
		ret[jss::transaction_result] = *TxResult;
		ret[jss::tx_status] = "validated";
        return ret;
	}

    auto tx = context.app.getMasterTransaction().fetch(txHash);
    if (nullptr != tx)
    {
        auto txn = tx->getSTransaction();
        if (tx->getStatus() != COMMITTED && 
            txn->isFieldPresent(sfLastLedgerSequence) && 
            txn->getFieldU32(sfLastLedgerSequence) <= context.app.getLedgerMaster().getValidLedgerIndex())
        {
            ret[jss::tx_status] = "failed";
        }
        else
        {
            if (tx->getStatus() == INCLUDED || tx->getStatus() == HELD ||
                tx->getStatus() == COMMITTED)
                ret[jss::tx_status] = "pending";
            else
                ret[jss::tx_status] = "failed";
        }
    }
    else
    {
	    ret[jss::tx_status] = "not_found";
    }
		
	return ret;
}

Json::Value
doTxCount(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return rpcError(rpcNOT_ENABLED);
    bool bChainsql = false;
    int ledger_index = -1;
    if (context.params.isMember(jss::ledger_index))
        ledger_index = context.params[jss::ledger_index].asInt();
    if (ledger_index == -1)
    {
        ledger_index = context.app.getLedgerMaster().getValidLedgerIndex();
    }
    if (context.params.isMember(jss::chainsql_tx))
        bChainsql = context.params[jss::chainsql_tx].asBool();
    Json::Value ret(Json::objectValue);
    if (bChainsql)
        ret["chainsql"] = context.app.getMasterTransaction().getTxCount(true,ledger_index);
    ret["all"] = context.app.getMasterTransaction().getTxCount(false, ledger_index);

    return ret;
}

Json::Value
doTxInPool(RPC::JsonContext& context)
{
    return context.app.getTxPool().txInPool();
}

Json::Value
doGetCrossChainTx(RPC::JsonContext& context)
{
    if (!context.params.isMember(jss::transaction_hash))
        return rpcError(rpcINVALID_PARAMS);

    auto const txid = context.params[jss::transaction_hash].asString();
    int ledgerIndex = 1;
    uint256 txHash = beast::zero;
    int limit = 1;
    bool bInclusive = true;

    try
    {
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
            auto txn = context.app.getMasterTransaction().fetch(txHash);
            if (!txn)
                return rpcError(rpcTXN_NOT_FOUND);

            auto txPair =
                getLedgerTxs(context, txn->getLedger(), txHash, bInclusive);
            if (txPair.second != "")
            {
                Json::Value json(Json::objectValue);
                json[jss::error_message] = txPair.second;
                return json;
            }

            appendTxJson(txPair.first, jvTxns, txn->getLedger(), leftCount);
            leftCount -= txPair.first.size();
            if (leftCount <= 0)
                return ret;

            startLedger = txn->getLedger() + 1;
        }

        // i should traverse from mCompleteLedgers
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
            appendTxJson(txPair.first, jvTxns, i, leftCount);
            leftCount -= txPair.first.size();
            if (leftCount <= 0)
                return ret;
        }

        // auto end = std::chrono::system_clock::now();
        // using duration_type =
        // std::chrono::duration<std::chrono::microseconds>; auto duration2 =
        // (end - start).count() * std::chrono::microseconds::period::num /
        // std::chrono::microseconds::period::den; JLOG(debugLog().fatal()) <<
        // "---getCrossChainTx cost time:"<< duration2 <<" ledgerSeqs from
        // "<<startLedger<<" to "<< maxSeq; std::cerr << "---getCrossChainTx
        // cost time:" << duration2 << " ledgerSeqs from " << startLedger << "
        // to " << maxSeq << std::endl;

        return ret;
    }
    catch (std::exception const&)
    {
        return rpcError(rpcINTERNAL);
    }
}

void
appendTxJson(
    const std::vector<std::shared_ptr<STTx>>& vecTxs,
    Json::Value& jvTxns,
    int ledgerSeq,
    int limit)
{
    int count = std::min(limit, (int)vecTxs.size());
    for (int i = 0; i < count; i++)
    {
        Json::Value& jvObj = jvTxns.append(Json::objectValue);
        auto pSTTx = vecTxs[i];
        jvObj[jss::tx] = pSTTx->getJson();
        jvObj[jss::tx][jss::ledger_index] = ledgerSeq;
    }
}

// get txs from a ledger
std::pair<std::vector<std::shared_ptr<STTx>>, std::string>
getLedgerTxs(
    RPC::JsonContext& context,
    int ledgerSeq,
    uint256 startHash,
    bool include)
{
    std::vector<std::shared_ptr<STTx>> vecTxs;
    auto ledger = context.ledgerMaster.getLedgerBySeq(ledgerSeq);
    if (ledger == NULL)
    {
        std::string error =
            "Get ledger " + to_string(ledgerSeq) + std::string(" failed");
        return std::make_pair(vecTxs, error);
    }
    bool bFound = false;
    for (auto const& item : ledger->txMap())
    {
        try
        {
            auto blob = SerialIter{item.data(), item.size()}.getVL();
            std::shared_ptr<STTx> pSTTX =
                std::make_shared<STTx>(SerialIter{blob.data(), blob.size()});
            if (pSTTX->isChainSqlTableType())
            {
                if (startHash != beast::zero && !bFound)
                {
                    if (pSTTX->getTransactionID() == startHash)
                    {
                        bFound = true;
                        if (!include)
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
            // JLOG(journal_.warn()) << "Txn " << item.key() << " throws";
        }
    }
    return std::make_pair(vecTxs, "");
}

}  // namespace ripple
