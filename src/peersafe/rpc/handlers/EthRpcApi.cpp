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
#include <peersafe/protocol/STETx.h>
#include <peersafe/protocol/Contract.h>
#include <ripple/app/tx/apply.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/app/util/Common.h>
#include <ripple/json/json_reader.h>

namespace ripple {

Json::Value
doEthChainId(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = toHexString(getChainID(context.app.openLedger().current()));
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
doNetVersion(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = std::to_string(getChainID(context.app.openLedger().current()));
    }
    catch (std::exception&)
    {
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
        jvResult[jss::result] = toHexString(validatedLedgerIndex);

    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x0";
    }
    return jvResult;
}

Json::Value
getTxByHash(std::string txHash, Schema& app)
{
    Json::Value jvResult;
    try {
        auto hash = from_hex_text<uint256>(txHash);
        auto txn = app.getMasterTransaction().fetch(hash);
        if (!txn)
            return formatEthError(defaultEthErrorCode, rpcTXN_NOT_FOUND);
        
        auto tx =
            std::dynamic_pointer_cast<STETx const>(txn->getSTransaction());
        if (!tx)
            return formatEthError(defaultEthErrorCode, "Not a eth tx.");

        auto ledger = app.getLedgerMaster().getLedgerBySeq(txn->getLedger());
        jvResult["hash"] = "0x" + txHash;
        jvResult["transactionIndex"] = "0x1";
        if (txn->getLedger() == 0)
        {
            jvResult["blockHash"] = Json::nullValue;
            jvResult["blockNumber"] = Json::nullValue;
        }
        else
        {
            jvResult["blockHash"] = "0x" + to_string(ledger->info().hash);
            jvResult["blockNumber"] = toHexString(txn->getLedger());
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
        jvResult["nonce"] = toHexString(tx->getFieldU32(sfSequence));
        jvResult["input"] = "0x" + toLowerStr(strHex(tx->getFieldVL(sfContractData)));
        jvResult["gas"] = toHexString(tx->getFieldU32(sfGas));

        auto rlpData = tx->getRlpData();
        RLP const rlp(bytesConstRef(rlpData.data(), rlpData.size()));
        auto gasPrice = rlp[1].toInt<u256>();
        auto value = rlp[4].toInt<u256>();
        auto v = rlp[6].toInt<u256>();
        auto r = rlp[7].toInt<u256>();
        auto s = rlp[8].toInt<u256>();
        jvResult["value"] = toHexString(value);
        jvResult["gasPrice"] = toHexString(gasPrice);
        jvResult["v"] = toHexString(v);
        jvResult["r"] = toHexString(r);
        jvResult["s"] = toHexString(s);
    } catch (std::exception& ex) {
        return formatEthError(defaultEthErrorCode, ex.what());
    }
    
    return jvResult;
}

void
formatLedgerFields(Json::Value& ethRetFormat, Json::Value  const& jvResultTemp, bool isExpend, Schema& app)
{
    // ethRetFormat["baseFeePerGas"] = "0x0";
    ethRetFormat["difficulty"] = "0x00";
    ethRetFormat["extraData"] = "0x00";
    ethRetFormat["gasLimit"] = "0x6691b7";
    ethRetFormat["gasUsed"] = "0x5208";
    ethRetFormat["hash"] = "0x" + toLowerStr(jvResultTemp["ledger_hash"].asString());
    ethRetFormat["logsBloom"] = "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    ethRetFormat["miner"] = "0x0000000000000000000000000000000000000000";
    ethRetFormat["mixHash"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
    ethRetFormat["nonce"] = "0x0000000000000000";
    ethRetFormat["number"] = toHexString(jvResultTemp["ledger_index"].asUInt64());
    ethRetFormat["parentHash"] = "0x" + jvResultTemp["ledger"]["parent_hash"].asString();
    ethRetFormat["receiptsRoot"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
    ethRetFormat["sha3Uncles"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
    ethRetFormat["size"] = "0x2e1";
    ethRetFormat["stateRoot"] = "0x" + jvResultTemp["ledger"]["account_hash"].asString();
    ethRetFormat["timestamp"] = toHexString(jvResultTemp["ledger"]["close_time"].asUInt64());
    ethRetFormat["totalDifficulty"] = "0x00";
    ethRetFormat["transactions"] = Json::arrayValue;
    Json::Value jvTxArray = jvResultTemp["ledger"]["transactions"];
    for(auto it = jvTxArray.begin(); it != jvTxArray.end(); it++)
    {
        Json::Value jvTxItem;
        if(isExpend)
        {
            jvTxItem = getTxByHash((*it).asString(), app);
        }
        else
        {
            jvTxItem = "0x" + toLowerStr((*it).asString());
        }
        if(!jvTxItem.isMember(jss::error))
            ethRetFormat["transactions"].append(jvTxItem);
    }
    ethRetFormat["transactionsRoot"] =
        "0x" + jvResultTemp["ledger"]["transaction_hash"].asString();
    ethRetFormat["uncles"] = Json::arrayValue;
}

Json::Value
getBlock(RPC::JsonContext& context, bool isExpand)
{
    Json::Value jvResultTemp;
    RPC::LedgerHandler lgHandler(context);
    auto status = lgHandler.check();
    if (status)
        status.inject(jvResultTemp);
    else
        lgHandler.writeResult(jvResultTemp);
    
    Json::Value ethRetFormat;
    formatLedgerFields(ethRetFormat, jvResultTemp, isExpand, context.app);
    return ethRetFormat;
}

Json::Value
doEthGetBlockByNumber(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        Json::Value chainsqlParams;
        std::string ledgerIndexStr = context.params["realParams"][0u].asString();
        ethLdgIndex2chainsql(chainsqlParams, ledgerIndexStr);
        
        chainsqlParams[jss::transactions] = true;
        bool isExpand = context.params["realParams"][1u].asBool();
        context.params = chainsqlParams;
        
        jvResult[jss::result] = getBlock(context, isExpand);
        
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x00";
    }
    return jvResult;
}

Json::Value
doEthGetBlockByHash(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        Json::Value chainsqlParams;
        std::string ledgerHashStr = context.params["realParams"][0u].asString().substr(2);
        chainsqlParams[jss::ledger_hash] = ledgerHashStr;
        chainsqlParams[jss::transactions] = true;
        bool isExpand = context.params["realParams"][1u].asBool();
        context.params = chainsqlParams;
        
        jvResult[jss::result] = getBlock(context, isExpand);
        
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x00";
    }
    return jvResult;
}

std::pair<std::shared_ptr<SLE const>, int>
getAccountData(RPC::JsonContext& context)
{
    try {
        auto optID =
            parseHex<AccountID>(context.params["realParams"][0u].asString());
        if (!optID)
            return std::make_pair(nullptr, rpcDST_ACT_MALFORMED);
        AccountID accountID = *optID;
        
        std::string ledgerIndexStr = context.params["realParams"][1u].asString();
        ethLdgIndex2chainsql(context.params, ledgerIndexStr);
        
        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        
        //check if the new openLedger not created.
        auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
        if(ledger == nullptr)
            return std::make_pair(nullptr, rpcINTERNAL);
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
    jvResult[jss::result] = "0x00";
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            Json::Value jvAccepted;
            RPC::injectSLE(jvAccepted, *(accDataRet.first));
                
            if(jvAccepted.isMember("Balance"))
            {
                jvResult[jss::result] =
                    dropsToWeiHex(jvAccepted["Balance"].asUInt64());
            }
        }
    }
    catch (std::exception&)
    {
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
            return formatEthError(defaultEthErrorCode,rpcTXN_BIGGER_THAN_MAXSIZE);
        }

        if (!ret || !ret->size())
            return formatEthError(defaultEthErrorCode, rpcINVALID_PARAMS);

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
            return formatEthError(defaultEthErrorCode, "Check validity failed.");
        }

        std::string reason2;
        auto tpTrans =
            std::make_shared<Transaction>(stpTrans, reason2, context.app);
        if (tpTrans->getStatus() != NEW)
        {
            jvResult[jss::result] = "0x00";
            return jvResult;
        }

        context.netOps.processTransaction(
            tpTrans, isUnlimited(context.role), true, NetworkOPs::FailHard::no);

        std::string txIdStr = to_string(stpTrans->getTransactionID());
        jvResult[jss::result] = "0x" + toLowerStr(txIdStr);
    }
    catch (std::exception& e)
    {
        JLOG(context.j.warn()) << "Exception when construct STETx:" << e.what();
        jvResult[jss::result] = "0x00";
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
            jvResult[jss::status] = "0x00";
            return jvResult;
        }
        auto tx = txn->getSTransaction();
        auto ledger = 
            context.app.getLedgerMaster().getLedgerBySeq(txn->getLedger());
        jvResult["transactionHash"] = "0x" + txHash;
        jvResult["transactionIndex"] = "0x01";
        if (ledger == nullptr)
        {
            jvResult["blockHash"] = Json::nullValue;
            jvResult["blockNumber"] = Json::nullValue;
        }
        else
        {
            jvResult["blockHash"] = "0x" + to_string(ledger->info().hash);
            jvResult["blockNumber"] = toHexString(txn->getLedger());
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

        jvResult["logs"] = Json::arrayValue;
        if (!txn->getMeta().empty())
        {
            auto meta = std::make_shared<TxMeta>(
                txn->getID(), txn->getLedger(), txn->getMeta());
            bool txSuccess = meta->getResultTER() == tesSUCCESS;
            auto key = keylet::account(accountID);
            try
            {
                auto node = meta->getAffectedNode(key.key);
                auto preBalance = node.getFieldObject(sfPreviousFields)
                                      .getFieldAmount(sfBalance)
                                      .zxc()
                                      .drops();
                auto finalBalance = node.getFieldObject(sfFinalFields)
                                        .getFieldAmount(sfBalance)
                                        .zxc()
                                        .drops();
                auto fee = tx->getFieldAmount(sfFee).zxc().drops();
                uint64_t value = 0;
                if (tx->isFieldPresent(sfContractValue) && txSuccess)
                    value = tx->getFieldAmount(sfContractValue).zxc().drops();
                if (ledger != nullptr)
                {
                    auto gasUsed = (preBalance - finalBalance - fee - value) *
                        std::uint64_t(1e3) / ledger->fees().gas_price;
                    jvResult["cumulativeGasUsed"] = toHexString(gasUsed);
                    jvResult["gasUsed"] = toHexString(gasUsed);
                }
                else
                {
                    jvResult["cumulativeGasUsed"] = "0x0";
                    jvResult["gasUsed"] = "0x0";
                }                
                
                Blob ctrLogData = meta->getContractLogData();
                if(!ctrLogData.empty())
                {
                    std::string ctrLogDataStr = std::string(ctrLogData.begin(), ctrLogData.end());
                    Json::Value jvLogs;
                    Json::Reader().parse(ctrLogDataStr, jvLogs);
                    for(auto it = jvLogs.begin(); it != jvLogs.end(); it++)
                    {
                        Json::Value jvLogItem;
                        jvLogItem["logIndex"] = toHexString(it.index());
                        jvLogItem["transactionIndex"] = jvResult["transactionIndex"];
                        jvLogItem["transactionHash"] = jvResult["transactionHash"];
                        jvLogItem["blockHash"] = jvResult["blockHash"];
                        jvLogItem["blockNumber"] = jvResult["blockNumber"];
                        jvLogItem["address"] = jvResult["to"];
                        jvLogItem["data"] = "0x" + (*it)["contract_data"].asString();
                        Json::Value jvLogItemTopics;
                        Json::Value jvLogTopics = (*it)["contract_topics"];
                        for(auto iter = jvLogTopics.begin(); iter != jvLogTopics.end(); iter++)
                        {
                            Json::Value jvTopic("0x" + toLowerStr((*iter).asString()));
                            jvLogItemTopics.append(jvTopic);
                        }
                        jvLogItem["topics"] = jvLogItemTopics;
                        jvLogItem["type"] = "mined";
                        jvResult["logs"].append(jvLogItem);
                    }
                }
            }
            catch (...)
            {
            }

            jvResult[jss::status] = txSuccess ? "0x1" : "0x0";
        }
        
        if (!tx->isFieldPresent(sfContractAddress) &&
            tx->isFieldPresent(sfContractData))
        {
            // calculate contract address
            auto newAddress = Contract::calcNewAddress(
                accountID, tx->getFieldU32(sfSequence));
            jvResult["contractAddress"] = "0x" + ethAddrChecksum(to_string(uint160(newAddress)));
        }
        jvResult["logsBloom"] = "0x" + to_string(base_uint<8 * 256>());
        jvResult["root"] = "0x" + to_string(uint256());
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
    Json::Value ret;
    try
    {
        std::string txHash =
            context.params["realParams"][0u].asString().substr(2);
        ret[jss::result] = getTxByHash(txHash, context.app);
    }
    catch (std::exception&)
    {
        ret[jss::result] = Json::objectValue;
    }
    
    return ret;
}

Json::Value
doEthGetTransactionCount(RPC::JsonContext& context)
{
    Json::Value jvResult;
    jvResult[jss::result] = "0x0";
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            Json::Value jvAccepted;
            RPC::injectSLE(jvAccepted, *(accDataRet.first));
                
            if(jvAccepted.isMember("Sequence"))
            {
                jvResult[jss::result] = toHexString(jvAccepted["Sequence"].asUInt64());
            }
        }            
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
doEthGasPrice(RPC::JsonContext& context)
{
    Json::Value jvResult;
    uint64_t gasPrice = 0;
    try
    {
        gasPrice = context.app.openLedger().current()->fees().gas_price;
    }
    catch (std::exception&)
    {}
    jvResult[jss::result] = compressDrops2Str(gasPrice);
    return jvResult;
}

Json::Value
doEthFeeHistory(RPC::JsonContext& context)
{
    Json::Value jvResult;
    jvResult[jss::result] = "0x";
    //try
    //{
    //    auto blockCount = (int64_t) std::stoll(
    //        context.params["realParams"][0u].asString().substr(2), 0, 16);
    //    auto gasPrice =
    //        dropsToWeiHex(context.app.openLedger().current()->fees().gas_price);

    //    auto maxVal = context.app.getLedgerMaster().getValidLedgerIndex(); 
    //    Json::Value& jvBaseFeePerGas =
    //        (jvResult[jss::result]["baseFeePerGas"] = Json::arrayValue);
    //    for (int i = 0; i <= blockCount && maxVal-i>0; i++)
    //    {
    //        jvBaseFeePerGas.append(gasPrice);
    //    }

    //    Json::Value& jvGasUsedRatio =
    //        (jvResult[jss::result]["gasUsedRatio"] = Json::arrayValue);
    //    for (int i = 0; i < blockCount && maxVal - i > 0; i++)
    //    {
    //        jvGasUsedRatio.append(0.99);
    //    }
    //    auto oldest = maxVal - blockCount > 0 ? maxVal - blockCount : 1;
    //    jvResult[jss::result]["oldestBlock"] = toHexString(oldest);
    //}
    //catch (std::exception&)
    //{
    //    //        jvResult = RPC::make_error(rpcINTERNAL,
    //    //            "Exception occurred during JSON handling.");
    //    jvResult[jss::result] = "0x";
    //}
    return jvResult;
}

Json::Value
doEthGetCode(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = "0x";
        auto accDataRet = getAccountData(context);

        if (accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            auto sle = *(accDataRet.first);

            if (sle.isFieldPresent(sfContractCode))
            {
                jvResult[jss::result] = "0x" + strHex(strCopy(sle.getFieldVL(sfContractCode)));
            }
        }
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

} // ripple
