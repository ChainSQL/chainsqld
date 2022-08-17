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
#include <peersafe/protocol/Contract.h>
#include <ripple/app/tx/apply.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/ledger/TransactionMaster.h>

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
        chainsqlParams[jss::ledger_index] = (int64_t)std::stoll(ledgerIndexStr, 0, 16);
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

std::pair<std::shared_ptr<SLE const>, int>
getAccountData(RPC::JsonContext& context)
{
    try {
        std::string addrStrHex = context.params["realParams"][0u].asString().substr(2);
        auto addrHex = *(strUnHex(addrStrHex));
        AccountID accountID;
        if (addrHex.size() != accountID.size())
            return std::make_pair(nullptr, rpcDST_ACT_MALFORMED);
        std::memcpy(accountID.data(), addrHex.data(), addrHex.size());
        
        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        
        //check if the new openLedger not created.
        auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
        if (ledger->open() && ledger->info().seq <= ledgerVal->info().seq)
            ledger = ledgerVal;

        return std::make_pair(ledger->read(keylet::account(accountID)), rpcSUCCESS);
    } catch (std::exception&) {
        return std::make_pair(nullptr, rpcINTERNAL);
    }
}

Json::Value
doEthGetBalance(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            Json::Value jvAccepted;
            RPC::injectSLE(jvAccepted, *(accDataRet.first));
                
            if(jvAccepted.isMember("Balance"))
            {
                boost::multiprecision::uint128_t balance;
                balance = boost::multiprecision::multiply(balance, jvAccepted["Balance"].asUInt64(), std::uint64_t(1e12));
                jvResult[jss::result] = (boost::format("0x%x") % (balance)).str();
            }
        }
        else if(accDataRet.second == rpcDST_ACT_MALFORMED)
            return rpcError(rpcDST_ACT_MALFORMED);
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
        auto stpTrans = std::make_shared<STETx>(makeSlice(*ret));


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
    catch (std::exception& e)
    {
        JLOG(context.j.warn()) << "Exception when construct STETx:" << e.what();
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
        auto hash = from_hex_text<uint256>(txHash);
        auto txn = context.app.getMasterTransaction().fetch(hash);
        if (!txn)
        {
            jvResult[jss::status] = 0;
            return jvResult;
        }
        auto tx = txn->getSTransaction();
        auto ledger =
            context.app.getLedgerMaster().getLedgerBySeq(txn->getLedger());
        jvResult["transactionHash"] = "0x" + txHash;
        jvResult["transactionIndex"] = 0;
        if (txn->getLedger() == 0)
        {
            jvResult["blockHash"] = Json::nullValue;
            jvResult["blockNumber"] = Json::nullValue;
        }
        else
        {
            jvResult["blockHash"] = "0x" + to_string(ledger->info().hash);
            jvResult["blockNumber"] = txn->getLedger();
        }
        auto accountID = tx->getAccountID(sfAccount);
        uint160 fromAccount = uint160(accountID);
        jvResult["from"] = "0x" + to_string(fromAccount);
        if (tx->isFieldPresent(sfContractAddress))
        {
            uint160 toAccount = uint160(tx->getAccountID(sfContractAddress));
            jvResult["to"] = "0x" + to_string(toAccount);
        }
        else
            jvResult["to"] = Json::nullValue;

        if (!txn->getMeta().empty())
        {
            auto meta = std::make_shared<TxMeta>(
                txn->getID(), txn->getLedger(), txn->getMeta());
            auto key = keylet::account(accountID);
            try
            {
                auto node = meta->getAffectedNode(key.key);
                auto preBalance = node.getFieldObject(sfPreviousFields)
                    .getFieldAmount(sfBalance).zxc().drops();
                auto finalBalance = 
                    node.getFieldObject(sfFinalFields)
                    .getFieldAmount(sfBalance).zxc().drops(); 
                auto fee = tx->getFieldAmount(sfFee).zxc().drops();
                uint64_t value = 0;
                if (tx->isFieldPresent(sfContractValue))
                    value = tx->getFieldAmount(sfContractValue).zxc().drops();
                auto gasPrice = (preBalance - finalBalance - fee - value) / ledger->fees().gas_price;
                jvResult["cumulativeGasUsed"] = gasPrice;
                jvResult["gasUsed"] = gasPrice;
            }
            catch (...)
            {
            }

            if (!tx->isFieldPresent(sfContractAddress) &&
                tx->isFieldPresent(sfContractData))
            {
                // calculate contract address
                auto newAddress = Contract::calcNewAddress(
                    accountID, tx->getFieldU32(sfSequence));
                jvResult["contractAddress"] = "0x" + to_string(uint160(newAddress));
            }
            jvResult["logs"] = Json::arrayValue;
            jvResult["logsBloom"] = "0x" + to_string(base_uint<8 * 256>());
            jvResult["root"] = "0x" + to_string(uint256());
            jvResult[jss::status] =  meta->getResultTER() == tesSUCCESS ? 1 : 0;
        }
    }
    catch (std::exception&)
    {
    }
    Json::Value ret;
    ret[jss::result] = jvResult;
    return ret;
}

Json::Value
doEthGetTransactionByHash(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        std::string txHash =
            context.params["realParams"][0u].asString().substr(2);
        auto hash = from_hex_text<uint256>(txHash);
        auto txn = context.app.getMasterTransaction().fetch(hash);
        if (!txn)
            return jvResult;
        auto tx =
            std::dynamic_pointer_cast<STETx const>(txn->getSTransaction());
        if (!tx)
            return jvResult;

        auto ledger =
            context.app.getLedgerMaster().getLedgerBySeq(txn->getLedger());
        jvResult["hash"] = "0x" + txHash;
        jvResult["transactionIndex"] = 0;
        if (txn->getLedger() == 0)
        {
            jvResult["blockHash"] = Json::nullValue;
            jvResult["blockNumber"] = Json::nullValue;
        }
        else
        {
            jvResult["blockHash"] = "0x" + to_string(ledger->info().hash);
            jvResult["blockNumber"] = txn->getLedger();
        }
        auto accountID = tx->getAccountID(sfAccount);
        uint160 fromAccount = uint160(accountID);
        jvResult["from"] = "0x" + to_string(fromAccount);
        if (tx->isFieldPresent(sfContractAddress))
        {
            uint160 toAccount = uint160(tx->getAccountID(sfContractAddress));
            jvResult["to"] = "0x" + to_string(toAccount);
        }
        else
            jvResult["to"] = Json::nullValue;
        jvResult["nonce"] = tx->getFieldU32(sfSequence);
        jvResult["input"] = "0x" + strHex(tx->getFieldVL(sfContractData));
        jvResult["gas"] = tx->getFieldU32(sfGas);

        auto rlpData = tx->getRlpData();
        RLP const rlp(bytesConstRef(rlpData.data(), rlpData.size()));
        auto gasPrice = rlp[1].toInt<u256>();
        auto value = rlp[4].toInt<u256>();
        auto v = rlp[6].toInt<u256>();
        auto r = rlp[7].toInt<u256>();
        auto s = rlp[8].toInt<u256>();
        jvResult["value"] = (boost::format("0x%x") % (value)).str();
        jvResult["gasPrice"] = (boost::format("0x%x") % (gasPrice)).str();
        jvResult["v"] = (boost::format("0x%x") % (v)).str();
        jvResult["r"] = (boost::format("0x%x") % (r)).str();
        jvResult["s"] = (boost::format("0x%x") % (s)).str();
    }
    catch (std::exception&)
    {
    }
    Json::Value ret;
    ret[jss::result] = jvResult;
    return ret;
}

Json::Value
doEthGetTransactionCount(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            Json::Value jvAccepted;
            RPC::injectSLE(jvAccepted, *(accDataRet.first));
                
            if(jvAccepted.isMember("Sequence"))
            {
                jvResult[jss::result] = (boost::format("0x%x") % (jvAccepted["Sequence"].asUInt64())).str();
            }
        }
        else if(accDataRet.second == rpcDST_ACT_MALFORMED)
            return rpcError(rpcDST_ACT_MALFORMED);
        else jvResult[jss::result] = "0x0";
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doEthEstimateGas(RPC::JsonContext& context)
{
    return doEstimateGas(context);
}

Json::Value
doEthCall(RPC::JsonContext& context)
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
doEthGasPrice(RPC::JsonContext& context)
{
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
doEthGasHistory(RPC::JsonContext& context)
{
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

} // ripple
