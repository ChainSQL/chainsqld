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
        Json::Value jvResultDetail;
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
doEthGetBalance(RPC::JsonContext& context)
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

} // ripple
