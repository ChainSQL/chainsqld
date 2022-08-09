//
//  EthRpcApi.cpp
//  chainsqld
//
//  Created by lascion on 2022/7/25.
//

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/handlers/LedgerHandler.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <peersafe/protocol/STETx.h>
#include <ripple/app/tx/apply.h>
#include <ripple/app/misc/Transaction.h>

namespace ripple {

Json::Value
doEthChainId(RPC::JsonContext& context)
{
//    Schema& appTemp = context.app;

    // Json::Value jsonRpcObj = context.params[jss::tx_json];
//    Json::Value jsonParams = context.params;

    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = "0x2ce";

    }
    catch (std::exception&)
    {
//        jvResult = RPC::make_error(rpcINTERNAL,
//            "Exception occurred during JSON handling.");
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doNetVersion(RPC::JsonContext& context)
{
//    Schema& appTemp = context.app;

    // Json::Value jsonRpcObj = context.params[jss::tx_json];
//    Json::Value jsonParams = context.params;

    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = "718";

    }
    catch (std::exception&)
    {
//        jvResult = RPC::make_error(rpcINTERNAL,
//            "Exception occurred during JSON handling.");
        jvResult[jss::result] = "0";
    }
    return jvResult;
}

Json::Value
doEthBlockNumber(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        auto validatedLedgerIndex = context.ledgerMaster.getValidLedgerIndex();
        jvResult[jss::result] = (boost::format("0x%x") % validatedLedgerIndex).str();

    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthGetBlockByNumber(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        Json::Value chainsqlParams;
        std::string ledgerIndexStr = context.params["realParams"][0u].asString().substr(2);
        chainsqlParams[jss::ledger_index] = beast::lexicalCastThrow<std::uint64_t>(ledgerIndexStr);
        chainsqlParams[jss::transactions] = true;
        chainsqlParams[jss::expand] = context.params["realParams"][1u].asBool();
        context.params = chainsqlParams;
        
        Json::Value jvResultTemp;
        RPC::LedgerHandler lgHandler(context);
        auto status = lgHandler.check();
        if (status)
            status.inject(jvResultTemp);
        else
            lgHandler.writeResult(jvResultTemp);
        
//        return status;
//        std::string addrStrHex = context.params["realParams"][0u].asString().substr(2);
//        Json::Value jvResultDetail;
//        auto validatedLedgerIndex = context.ledgerMaster.getValidLedgerIndex();
//        jvResult[jss::result] = (boost::format("0x%x") % validatedLedgerIndex).str();
        Json::Value ethRetFormat;
        ethRetFormat["baseFeePerGas"] = "0x0";
        ethRetFormat["difficulty"] = "0x2";
        ethRetFormat["extraData"] = "0x0";
        ethRetFormat["gasLimit"] = "0x0";
        ethRetFormat["gasUsed"] = "0x0";
        ethRetFormat["hash"] = jvResultTemp["ledger_hash"];
        ethRetFormat["logsBloom"] = "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
        ethRetFormat["miner"] = "0x0000000000000000000000000000000000000000";
        ethRetFormat["mixHash"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
        ethRetFormat["nonce"] = "0x0000000000000000";
        ethRetFormat["number"] = (boost::format("0x%x") % jvResultTemp["ledger_index"].asUInt64()).str();
        ethRetFormat["parentHash"] = jvResultTemp["ledger"]["parent_hash"];
        ethRetFormat["receiptsRoot"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
        ethRetFormat["sha3Uncles"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
        ethRetFormat["size"] = "0x2e1";
        ethRetFormat["stateRoot"] = jvResultTemp["ledger"]["account_hash"];
        ethRetFormat["timestamp"] = (boost::format("0x%x") % jvResultTemp["ledger"]["close_time"].asUInt64()).str();
        ethRetFormat["totalDifficulty"] = "0x7";
        ethRetFormat["transactions"] = jvResultTemp["ledger"]["transactions"];
        ethRetFormat["transactionsRoot"] = jvResultTemp["ledger"]["transaction_hash"];
        ethRetFormat["uncles"] = Json::arrayValue;
        jvResult[jss::result] = ethRetFormat;
        
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthGetBalance(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        std::string addrStrHex = context.params["realParams"][0u].asString().substr(2);
        auto addrHex = *(strUnHex(addrStrHex));
        AccountID accountID;
        if (addrHex.size() != accountID.size())
            return rpcError(rpcDST_ACT_MALFORMED);
        std::memcpy(accountID.data(), addrHex.data(), addrHex.size());
        
        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        
        //check if the new openLedger not created.
        auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
        if (ledger->open() && ledger->info().seq <= ledgerVal->info().seq)
            ledger = ledgerVal;

        auto const sleAccepted = ledger->read(keylet::account(accountID));
        if(sleAccepted)
        {
            Json::Value jvAccepted;
            RPC::injectSLE(jvAccepted, *sleAccepted);
            
            if(jvAccepted.isMember("Balance"))
            {
                boost::multiprecision::uint128_t balance;
                balance = boost::multiprecision::multiply(balance, jvAccepted["Balance"].asUInt64(), std::uint64_t(1e12));
                jvResult[jss::result] = (boost::format("0x%x") % (balance)).str();
            }
        }
        else jvResult[jss::result] = "0x0";
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthSendRawTransaction(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        auto ret =
            strUnHex(context.params["realParams"][0u].asString().substr(2));

        // 500KB
        if (ret->size() > RPC::Tuning::max_txn_size)
        {
            return rpcError(rpcTXN_BIGGER_THAN_MAXSIZE);
        }

        if (!ret || !ret->size())
            return rpcError(rpcINVALID_PARAMS);

        //Construct STETx
        auto stpTrans = std::make_shared<STETx const>(makeSlice(*ret));

        //Check validity
        auto [validity, reason] = checkValidity(
            context.app,
            context.app.getHashRouter(),
            *stpTrans,
            context.ledgerMaster.getCurrentLedger()->rules(),
            context.app.config());
        if (validity != Validity::Valid)
        {

            return jvResult;
        }

        std::string reason2;
        auto tpTrans =
            std::make_shared<Transaction>(stpTrans, reason2, context.app);
        if (tpTrans->getStatus() != NEW)
        {
            jvResult[jss::result] = "0x0";
            return jvResult;
        }

        context.netOps.processTransaction(
            tpTrans, isUnlimited(context.role), true, NetworkOPs::FailHard::no);

        jvResult[jss::result] = "0x" + to_string(stpTrans->getTransactionID());
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthGetTransactionReceipt(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        std::string txHash =
            context.params["realParams"][0u].asString().substr(2);
        
        jvResult[jss::result] = "0x0";
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthGetTransactionByHash(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        std::string txHash =
            context.params["realParams"][0u].asString().substr(2);

        jvResult[jss::result] = "0x0";
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

} // ripple
