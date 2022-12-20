//
//  EthRpcApi.cpp
//  chainsqld
//
//  Created by lascion on 2022/7/25.
//
#include <sstream>

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
#include <eth/api/utils/Helpers.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/app/tx/impl/Tuning.h>
#include <ripple/json/json_reader.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/Feature.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/protocol/STMap256.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <ripple/protocol/SecretKey.h>
#include <peersafe/app/bloom/Filter.h>
#include <peersafe/app/bloom/FilterApi.h>
#include <peersafe/app/bloom/BloomManager.h>
#include <peersafe/app/misc/Executive.h>

namespace ripple {

const std::string ETH_ERROR_NUM_RETURN = "0x00";
const std::string ETH_ERROR_OBJ_RETURN = "0x";

std::shared_ptr<ReadView const>
getLedgerByParam(RPC::JsonContext& context,std::string const& ledgerParamStr)
{
    if (ledgerParamStr.length() > 64)
        context.params[jss::ledger_hash] = ledgerParamStr.substr(2);
    else
        ethLdgIndex2chainsql(context.params, ledgerParamStr);
    std::shared_ptr<ReadView const> ledger;
    RPC::lookupLedger(ledger, context);
    return ledger;
}

Json::Value
doWeb3CleintVersion(RPC::JsonContext& context)
{
    Json::Value jvResult;
    std::string version = BuildInfo::getVersionString();
    jvResult[jss::result] = "chainsql/" + version + "/linux-amd64/C++17";

    return jvResult;
}

Json::Value
doWeb3Sha3(RPC::JsonContext& context)
{
    Json::Value jvResult;
    jvResult[jss::result] = ETH_ERROR_NUM_RETURN;
    try
    {
        if (context.params["realParams"].size()>0)
        {
            std::string param =
                context.params["realParams"][0u].asString().substr(2);
            auto data = strUnHex(param);
            if (data)
            {
                jvResult[jss::result] =
                    "0x" + to_string(sha512Half<CommonKey::sha3>(*data));
            }
        }
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

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
        jvResult[jss::result] = ETH_ERROR_NUM_RETURN;
    }
    return jvResult;
}

Json::Value
doNetVersion(RPC::JsonContext& context)
{
    //This interface will decimal string,not hex!
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = to_string(getChainID(context.app.openLedger().current()));
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0";
    }
    return jvResult;
}

Json::Value
doNetPeerCount(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = toHexString(context.app.peerManager().size());
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = "0x00";
    }
    return jvResult;
}


Json::Value
doNetListening(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = context.netOps.getServerStatus() == "normal";
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = false;
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
        jvResult[jss::result] = ETH_ERROR_NUM_RETURN;
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
            return formatEthError(ethERROR_DEFAULT, rpcTXN_NOT_FOUND);
        
        auto tx =
            std::dynamic_pointer_cast<STETx const>(txn->getSTransaction());
        if (!tx)
            return formatEthError(ethERROR_DEFAULT, "Not a eth tx.");

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
        return formatEthError(ethERROR_DEFAULT, ex.what());
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
        jvResult[jss::result] = ETH_ERROR_OBJ_RETURN;
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
        jvResult[jss::result] = ETH_ERROR_OBJ_RETURN;
    }
    return jvResult;
}


std::pair<std::shared_ptr<SLE const>, int>
getAccountData(RPC::JsonContext& context)
{
    try
    {
        auto optID =
            parseHex<AccountID>(context.params["realParams"][0u].asString());
        if (!optID)
            return std::make_pair(nullptr, rpcDST_ACT_MALFORMED);
        AccountID accountID = *optID;
        auto ledger =
            getLedgerByParam(context,context.params["realParams"][1u].asString());
        
        // check if the new openLedger not created.
        auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
        if (ledger == nullptr)
            return std::make_pair(nullptr, rpcINTERNAL);
        if (ledger->open() && ledger->info().seq <= ledgerVal->info().seq)
            ledger = ledgerVal;

        return std::make_pair(
            ledger->read(keylet::account(accountID)), rpcSUCCESS);
    }
    catch (std::exception&)
    {
        return std::make_pair(nullptr, rpcINTERNAL);
    }
}

Json::Value
doEthGetBalance(RPC::JsonContext& context)
{
    Json::Value jvResult;
    jvResult[jss::result] = ETH_ERROR_NUM_RETURN;
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            auto balance =
                (*accDataRet.first).getFieldAmount(sfBalance).zxc().drops();
            jvResult[jss::result] = dropsToWeiHex(balance);
        }
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
doEthGetTransactionReceipt(RPC::JsonContext& context)
{
    Json::Value jvResult(Json::objectValue);
    try
    {
        std::string txHash =
            context.params["realParams"][0u].asString().substr(2);
        auto hash = from_hex_text<uint256>(txHash);
        auto txn = context.app.getMasterTransaction().fetch(hash);
        if (!txn)
        {
            jvResult[jss::status] = ETH_ERROR_OBJ_RETURN;
            return jvResult;
        }
        auto tx = txn->getSTransaction();
        if (tx->getTxnType() != ttETH_TX)
            return jvResult;

        auto ledger = 
            context.app.getLedgerMaster().getLedgerBySeq(txn->getLedger());
        jvResult["transactionHash"] = "0x" + txHash;
        jvResult["transactionIndex"] = "0x01";
        if (ledger == nullptr)
        {
            jvResult["blockHash"] = Json::nullValue;
            jvResult["blockNumber"] = Json::nullValue;
            jvResult["logsBloom"] = "0x" + to_string(base_uint<8 * 256>());
        }
        else
        {
            jvResult["blockHash"] = "0x" + to_string(ledger->info().hash);
            jvResult["blockNumber"] = toHexString(txn->getLedger());
            jvResult["logsBloom"] = "0x" + to_string(ledger->info().bloom);
        }
        auto accountID = tx->getAccountID(sfAccount);
        uint160 fromAccount = uint160(accountID);
        auto contractAccount = *getContractAddress(*tx);
        std::string contractAddress = "0x" + to_string(uint160(contractAccount));
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
                    std::uint64_t gasUsed = preBalance - finalBalance - fee - value;
                    if(ledger->rules().enabled(featureGasPriceCompress))
                        gasUsed = gasUsed * compressDrop / ledger->fees().gas_price;
                    else gasUsed = gasUsed / ledger->fees().gas_price;
                    
                    jvResult["cumulativeGasUsed"] = toHexString(gasUsed);
                    jvResult["gasUsed"] = toHexString(gasUsed);
                }
                else
                {
                    jvResult["cumulativeGasUsed"] = ETH_ERROR_NUM_RETURN;
                    jvResult["gasUsed"] = ETH_ERROR_NUM_RETURN;
                }                
                
                Blob ctrLogData = meta->getContractLogData();
                if(!ctrLogData.empty())
                {
                    std::string ctrLogDataStr = std::string(ctrLogData.begin(), ctrLogData.end());
                    Json::Value jvLogs;
                    Json::Reader().parse(ctrLogDataStr, jvLogs);
                    jvResult["logs"] =
                        parseContractLogs(jvLogs, contractAddress, jvResult);
                }
            }
            catch (...)
            {
            }

            jvResult[jss::status] = txSuccess ? "0x01" : ETH_ERROR_NUM_RETURN;
        }
        
        if (!tx->isFieldPresent(sfContractAddress) &&
            tx->isFieldPresent(sfContractData))
        {
            jvResult["contractAddress"] =
                "0x" + ethAddrChecksum(contractAddress);
        }
        
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
    jvResult[jss::result] = ETH_ERROR_NUM_RETURN;
    try
    {
        auto accDataRet = getAccountData(context);

        if(accDataRet.second == rpcSUCCESS && accDataRet.first)
        {
            auto sequence = (*accDataRet.first).getFieldU32(sfSequence);
            jvResult[jss::result] = toHexString(sequence);
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
    jvResult[jss::result] =
        compressDrops2Str(gasPrice, 
            context.app.openLedger().current()->rules().enabled(featureGasPriceCompress));
    return jvResult;
}

Json::Value
doEthGetCode(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        jvResult[jss::result] = ETH_ERROR_OBJ_RETURN;
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

Json::Value
doEthMining(RPC::JsonContext& context)
{
    Json::Value jvResult;

    jvResult[jss::result] = false;
    
    return jvResult;
}

Json::Value
doEthAccounts(RPC::JsonContext& context)
{
    Json::Value jvResult;
    jvResult[jss::result] = Json::arrayValue;
    try
    {
        auto oSecret = context.app.config().ETH_DEFAULT_ACCOUNT_PRIVATE;
        if (oSecret)
        {
            auto address = addressFromSecret(*oSecret);
            jvResult[jss::result].append("0x" + address.hex());
        }
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
doEthGetStorageAt(RPC::JsonContext& context)
{
    Json::Value jvResult(Json::objectValue);
    jvResult[jss::result] = ETH_ERROR_OBJ_RETURN;
    try
    {
        //get the right ledger
        auto ledger = getLedgerByParam(
            context, context.params["realParams"][2u].asString());
        if(ledger == nullptr)     
            return jvResult;
        
        //get contract sle
        auto optID =parseHex<AccountID>(context.params["realParams"][0u].asString());
        if (!optID)               return jvResult;
        auto pSle = ledger->read(keylet::account(*optID));
        if(pSle == nullptr)       return jvResult;
        auto const&  mapStore = pSle->getFieldM256(sfStorageOverlay);
        
        //get info from contract-helper
        uint256 uKey = from_hex_text<uint256>(context.params["realParams"][1u].asString());
        ContractHelper& helper = context.app.getContractHelper();
        auto value = helper.fetchFromDB(*optID, mapStore.rootHash(), uKey, true);
        uint256 retValue = value ? *value : beast::zero;
        jvResult[jss::result] = "0x" + to_string(retValue);
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
doEthSign(RPC::JsonContext& context)
{
    Json::Value jvResult(Json::objectValue);
    try
    {
        auto address = context.params["realParams"][0u].asString();
        auto param = context.params["realParams"][1u].asString().substr(2);
        auto optData = strUnHex(param);
        if (optData)
        {
            std::string secret =
                *context.app.config().ETH_DEFAULT_ACCOUNT_PRIVATE;
            if (eth::h160(address) != addressFromSecret(secret))
            {
                return formatEthError(
                    ethERROR_DEFAULT,
                    "Address not matching configured private-key.");
            }

            auto data = *optData;
            std::string signData =
                boost::str(boost::format("\x19%s%d%s") % "Ethereum Signed Message:\n" % data.size() %
                std::string(data.begin(), data.end()).c_str());
            auto digest = sha512Half<CommonKey::sha3>(signData);

            auto priData = *strUnHex(secret.substr(2));
            SecretKey sk(Slice(priData.data(), priData.size()));
            auto sig = signEthDigest(sk, digest);
            SignatureStruct sigStruct = *(SignatureStruct const*)&sig;
            if (sigStruct.isValid())
            {
                sigStruct.v += 27;
            }
            Blob ret(sig.begin(), sig.end());
            jvResult[jss::result] = "0x" + strHex(ret);
        }
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
getTxCountBySeq(
    RPC::JsonContext& context,
    int64_t seq)
{
    Json::Value jvResult(Json::objectValue);
    std::string prefix = "SELECT COUNT(*) FROM Transactions WHERE ";
    std::string sql = boost::str(
        boost::format(prefix + (R"(LedgerSeq = %d)")) %
        seq);
    sql += ";";
    boost::optional<int> txCount;
    {
        auto db = context.app.getTxnDB().checkoutDbRead();
        *db << sql, soci::into(txCount);

        if (!db->got_data() || !txCount)
            return jvResult;
    }
    jvResult[jss::result] = toHexString(*txCount);
    return jvResult;
}

Json::Value
doEthTxCountByHash(RPC::JsonContext& context)
{
    Json::Value jvResult = Json::objectValue;
    try
    {
        auto ledger =
            getLedgerByParam(context, context.params["realParams"][0u].asString());
        if (ledger == nullptr)
            return jvResult;

        return getTxCountBySeq(context, ledger->info().seq);
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
doEthTxCountByNumber(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return formatEthError(ethERROR_DEFAULT, rpcNOT_ENABLED);

    Json::Value jvResult(Json::objectValue);
    try
    {
        auto sSequence = context.params["realParams"][0u].asString();
        auto seq = (int64_t)std::stoll(sSequence.substr(2), 0, 16);

        return getTxCountBySeq(context,seq);
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

Json::Value
getTransactionBySeqAndIndex(RPC::JsonContext& context,int64_t seq,int64_t index)
{
    if (!context.app.config().useTxTables())
        return formatEthError(ethERROR_DEFAULT, rpcNOT_ENABLED);

    Json::Value jvResult(Json::objectValue);
    std::string prefix = "SELECT TransID FROM AccountTransactions where ";
    std::string sql = boost::str(boost::format(prefix +
                (R"(LedgerSeq = %d AND TxnSeq = %d)"))
                % seq
                % index);
    sql += ";";
    boost::optional<std::string> txID;
    {
        auto db = context.app.getTxnDB().checkoutDbRead();
        *db << sql, soci::into(txID);

        if (!db->got_data() || !txID)
            return jvResult;
    }
    jvResult[jss::result] = getTxByHash(*txID, context.app);
    return jvResult;
}

Json::Value
doEthTxByHashAndIndex(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return formatEthError(ethERROR_DEFAULT,rpcNOT_ENABLED);

    Json::Value jvResult(Json::objectValue);
    try
    {
        auto ledger = getLedgerByParam(
            context, context.params["realParams"][0u].asString());
        if (ledger == nullptr)
            return jvResult;
        auto seq = ledger->info().seq;
        auto sIndex = context.params["realParams"][1u].asString();
        auto index = (int64_t)std::stoll(sIndex.substr(2), 0, 16);
        
        return getTransactionBySeqAndIndex(context, seq, index);
    }
    catch (std::exception&)
    {
        jvResult[jss::result] = Json::objectValue;
    }
    return jvResult;
}

Json::Value
doEthTxByNumberAndIndex(RPC::JsonContext& context)
{
    if (!context.app.config().useTxTables())
        return formatEthError(ethERROR_DEFAULT, rpcNOT_ENABLED);

    Json::Value jvResult(Json::objectValue);
    try
    {
        auto sSequence = context.params["realParams"][0u].asString();
        auto seq = (int64_t)std::stoll(sSequence.substr(2), 0, 16);
        auto sIndex = context.params["realParams"][1u].asString();
        auto index = (int64_t)std::stoll(sIndex.substr(2), 0, 16);

        return getTransactionBySeqAndIndex(context, seq, index);
    }
    catch (std::exception&)
    {
    }
    return jvResult;
}

void retriveAddressesAndTopics(const Json::Value& params,
                               std::vector<uint160>& addresses,
                               std::vector<std::vector<uint256>>& topics) {
    
    Json::Value addressObject = params["address"];
    
    // retrive addresses
    if(addressObject.isString()) {
        std::string address_str = addressObject.asString();
        if(address_str[0] == '0'
           && (address_str[1] == 'x' || address_str[1] == 'X')) {
            address_str = address_str.substr(2);
        }
        addresses.push_back(from_hex_text<uint160>(address_str));
    } else if (addressObject.isArray()) {
        for(auto const& address: addressObject) {
            assert(address.isString());
            std::string address_str = address.asString();
            if(address_str[0] == '0'
               && (address_str[1] == 'x' || address_str[1] == 'X')) {
                address_str = address_str.substr(2);
            }
            addresses.push_back(from_hex_text<uint160>(address_str));
        }
    }
    
    // retrive topics
    Json::Value topicsObject = params["topics"];
    
    for(auto const& item: topicsObject) {
        if (item.isString()) {
            std::string topicStr = item.asString();
            if(topicStr[0] == '0'
               && (topicStr[1] == 'x' || topicStr[1] == 'X')) {
                topicStr = topicStr.substr(2);
            }
            std::vector<uint256> topic;
            topic.push_back(from_hex_text<uint256>(topicStr));
            topics.push_back(topic);
        } else if (item.isArray()) {
            std::vector<uint256> topic;
            for(auto const& t: item) {
                std::string topicStr = t.asString();
                if(topicStr[0] == '0'
                   && (topicStr[1] == 'x' || topicStr[1] == 'X')) {
                    topicStr = topicStr.substr(2);
                }
                topic.push_back(from_hex_text<uint256>(topicStr));
            }
            topics.push_back(topic);
        }
    }
}

FilterQuery convertJsonParam(RPC::JsonContext& context) {
    FilterQuery query;
    Json::Value params = context.params["realParams"][0u];
    
    if(params["blockHash"] != Json::Value()) {
        std::string hashStr = params["blockHash"].asString().substr(2);
        query.blockHash = from_hex_text<uint256>(hashStr);
        retriveAddressesAndTopics(params, query.addresses, query.topics);
    } else {
        LedgerIndex fromBlock = context.app.getLedgerMaster().getValidatedLedger()->info().seq;
        LedgerIndex toBlock = fromBlock;
        
        if(params["fromBlock"].isNumeric()) {
            fromBlock = params["fromBlock"].asUInt();
        } else if (params["fromBlock"].isString()) {
            Json::Value query;
            RPC::JsonContext queryContext = context;
            ethLdgIndex2chainsql(query, params["fromBlock"].asString());
            queryContext.params = query;
            std::shared_ptr<ReadView const> ledger;
            RPC::lookupLedger(ledger, queryContext);
            if(ledger) {
                fromBlock = ledger->info().seq;
            }
        }
        
        if(params["toBlock"].isNumeric()) {
            toBlock = params["toBlock"].asUInt();
        } else if (params["toBlock"].isString()) {
            Json::Value query;
            RPC::JsonContext queryContext = context;
            ethLdgIndex2chainsql(query, params["toBlock"].asString());
            queryContext.params = query;
            std::shared_ptr<ReadView const> ledger;
            RPC::lookupLedger(ledger, queryContext);
            if(ledger) {
                toBlock = ledger->info().seq;
            }
        }
        
        query.fromBlock = fromBlock;
        query.toBlock = toBlock;
        
        retriveAddressesAndTopics(params, query.addresses, query.topics);
    }
    return query;
}

Json::Value
doEthGetLogs(RPC::JsonContext& context) {
    
    Json::Value result;
    Json::Value logs;
    bool bok = false;
    result[jss::result] = Json::Value(Json::arrayValue);

    auto bloomStartSeq = context.app.getBloomManager().getBloomStartSeq();
    if(!bloomStartSeq) {
        // Don't support bloom feature
        return result;
    }
    
    Filter::pointer filter = nullptr;
    FilterQuery query = convertJsonParam(context);
    if(query.blockHash != beast::zero) {
        filter = Filter::newBlockFilter(context.app,
                                        query.blockHash,
                                        query.addresses,
                                        query.topics);
    } else {
        
        if(query.toBlock < *bloomStartSeq) {
            // query block is less than bloomStartSeq,
            // which don't support bloom feature
            return result;
        }
        filter = Filter::newRangeFilter(context.app,
                                        query.fromBlock,
                                        query.toBlock,
                                        query.addresses,
                                        query.topics);
    }
    
    std::tie(logs, bok) = filter->Logs();
    if(bok) {
        result[jss::result] = logs;
    }
    return result;
}

Json::Value
doEthGetFilterLogs(RPC::JsonContext& context) {
    return doEthGetFilterChanges(context);
}

Json::Value
doEthGetFilterChanges(RPC::JsonContext& context) {
    Json::Value result;
    Json::Value params = context.params["realParams"][0u];
    assert(params.isString());
    std::string id = params.asString();
    FilterApi::FilterWrapper::pointer filter =context.app.getBloomManager().filterApi().getFilter(id);
    if(filter) {
        result[jss::result] = filter->result();
    } else {
        result[jss::result] = Json::Value(Json::arrayValue);
    }
    
    return result;
}

Json::Value
doEthNewFilter(RPC::JsonContext& context) {
    Json::Value result;
    FilterQuery query = convertJsonParam(context);
    std::string id = context.app.getBloomManager().filterApi().installFilter(query);
    result[jss::result] = id;
    return result;
}

Json::Value
doEthNewPendingTransactionFilter(RPC::JsonContext& context) {
    Json::Value result;
    std::string id = context.app.getBloomManager().filterApi().installPendingTransactionFilter();
    result[jss::result] = id;
    return result;
}

Json::Value
doEthNewBlockFilter(RPC::JsonContext& context) {
    Json::Value result;
    std::string id = context.app.getBloomManager().filterApi().installBlockFilter();
    result[jss::result] = id;
    return result;
}

Json::Value
doEthUninstallFilter(RPC::JsonContext& context) {
    Json::Value result;
    Json::Value params = context.params["realParams"][0u];
    assert(params.isString());
    std::string id = params.asString();
    bool released = context.app.getBloomManager().filterApi().uninstallFilter(id);
    result[jss::result] = released;
    return result;
}

Json::Value
checkEthJsonFields(Json::Value originJson)
{
    Json::Value ret = Json::Value();

    if (!originJson.isObject())
    {
        ret = RPC::object_field_error(jss::params);
    }

    if (!originJson.isMember("to"))
    {
        ret = RPC::missing_field_error("to");
    }

    if (!originJson.isMember("data"))
    {
        ret = RPC::missing_field_error("data");
    }
    return ret;
}

Json::Value
doEthCall(RPC::JsonContext& context)
{
    Schema& appTemp = context.app;
    Json::Value jsonParams = context.params;
    Json::Value ethParams = jsonParams["realParams"][0u];
    std::string ledgerIndexStr = context.params["realParams"][1u].asString();

    ethLdgIndex2chainsql(context.params, ledgerIndexStr);
    Json::Value checkResult = checkEthJsonFields(ethParams);
    if (isRpcError(checkResult))
        return formatEthError(
            ethERROR_DEFAULT, checkResult[jss::error_message].asString());

    AccountID accountID;
    if (ethParams.isMember("from"))
    {
        auto accId = parseHex<AccountID>(ethParams["from"].asString());
        if (!accId)
            return formatEthError(ethERROR_DEFAULT, rpcDST_ACT_MALFORMED);
        accountID = *accId;
    }

    std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);
    if (!ledger)
    {
        if (result.isMember(jss::error_message))
            return formatEthError(
                ethERROR_DEFAULT, result[jss::error_message].asString());
        return formatEthError(
                ethERROR_DEFAULT,"Get ledger failed.");
    }
    AccountID contractAddrID;
    {
        auto optID = parseHex<AccountID>(ethParams["to"].asString());
        if (!optID)
            return formatEthError(ethERROR_DEFAULT, rpcDST_ACT_MALFORMED);
        contractAddrID = *optID;

        if (!ethParams.isMember("from"))
            accountID = *optID;
    }
    auto strUnHexRes = strUnHex(ethParams["data"].asString().substr(2));
    if (!strUnHexRes || (*strUnHexRes).empty())
    {
        return formatEthError(ethERROR_DEFAULT, "contract_data is invalid.");
    }

    auto contractDataBlob = *strUnHexRes;

    // int64_t txValue = 0;
    STTx contractTx(
        ttCONTRACT,
        [&accountID, &contractAddrID, /*&txValue,*/ &contractDataBlob](
            auto& obj) {
            obj.setAccountID(sfAccount, accountID);
            obj.setAccountID(sfContractAddress, contractAddrID);
            obj.setFieldVL(sfContractData, contractDataBlob);
            obj.setFieldU16(sfContractOpType, QueryCall);
            // obj.setFieldAmount(sfAmount, ZXCAmount(txValue));
        });

    std::shared_ptr<OpenView> openViewTemp =
        std::make_shared<OpenView>(ledger.get());

    ApplyContext applyContext(
        appTemp,
        *openViewTemp,
        contractTx,
        tesSUCCESS,
        FeeUnit64{(std::uint64_t)openViewTemp->fees().base.drops()},
        tapNO_CHECK_SIGN,
        appTemp.journal("ContractLocalCall"));

    SleOps ops(applyContext);
    auto pInfo = std::make_shared<EnvInfoImpl>(
        applyContext.view().info().seq,
        TX_GAS,
        applyContext.view().fees().drops_per_byte,
        0,
        getChainID(context.app.openLedger().current()),
        true,
        applyContext.app.getPreContractFace());
    Executive e(ops, *pInfo, INITIAL_DEPTH);
    e.initialize();
    bool callResult = !(e.call(
        contractAddrID,
        accountID,
        uint256(),
        uint256(),
        &contractDataBlob,
        maxInt64 / 2));
    if (callResult)
        e.go();

    Json::Value jvResult;
    TER terResult = e.getException();
    eth::owning_bytes_ref localCallRet = e.takeOutput();
    if (terResult == tefCONTRACT_REVERT_INSTRUCTION)
    {
        jvResult = formatEthError(
            ethERROR_REVERTED, "0x" + strHex(e.takeRevertData().takeBytes()), revertMsg(localCallRet.toString()));
    }
    else
        jvResult[jss::result] = "0x" + strHex(localCallRet.takeBytes());

    return jvResult;
}

std::tuple <TER, eth::owning_bytes_ref, eth::owning_bytes_ref>
    execute(
    RPC::JsonContext& context,
    STTx const& contractTx,
    std::shared_ptr<OpenView> openViewTemp)
{
    ApplyContext applyContext(
        context.app,
        *openViewTemp,
        contractTx,
        tesSUCCESS,
        FeeUnit64{(std::uint64_t)openViewTemp->fees().base.drops()},
        tapNO_CHECK_SIGN,
        context.app.journal("EstimateGas"));

    SleOps ops(applyContext);
    auto pInfo = std::make_shared<EnvInfoImpl>(
        applyContext.view().info().seq,
        contractTx.getFieldU32(sfGas),
        applyContext.view().fees().drops_per_byte,
        0,
        getChainID(context.app.openLedger().current()),
        true,
        applyContext.app.getPreContractFace());
    Executive e(ops, *pInfo, INITIAL_DEPTH);
    e.initialize();

    TER execRet = tesSUCCESS;
    if (!e.execute())
    {
        e.go();
    }
    return std::make_tuple(
        e.getException(), e.takeOutput(), e.takeRevertData());
}

Json::Value
doEstimateGas(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        // This is a high burden interface
        context.loadType = Resource::feeHighBurdenRPC;

        Schema& appTemp = context.app;
        Json::Value jsonParams = context.params;
        Json::Value ethParams = jsonParams["realParams"][0u];

        AccountID accountID;

        auto optID = RPC::accountFromStringStrict(ethParams["from"].asString());
        if (!optID)
            return formatEthError(ethERROR_DEFAULT, rpcDST_ACT_MALFORMED);

        accountID = *optID;

        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        if (!ledger)
            return result;
        if (!ledger->exists(keylet::account(accountID)))
            return formatEthError(ethERROR_DEFAULT, rpcACT_NOT_FOUND);

        std::shared_ptr<OpenView> openViewTemp =
            std::make_shared<OpenView>(ledger.get());

        AccountID contractAddrID;
        bool isCreation = true;
        bool isCtrAddr = false;
        if (ethParams.isMember("to"))
        {
            auto optID = parseHex<AccountID>(ethParams["to"].asString());
            if (!optID)
                return formatEthError(ethERROR_DEFAULT, rpcDST_ACT_MALFORMED);

            contractAddrID = *optID;
            isCreation = false;

            // check if the new openLedger not created.
            auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
            if (ledger->open() && ledger->info().seq <= ledgerVal->info().seq)
                ledger = ledgerVal;

            if (ledger->exists(keylet::account(contractAddrID)))
            {
                auto ctrSle = *(ledger->read(keylet::account(contractAddrID)));

                if (ctrSle.isFieldPresent(sfContractCode))
                {
                    isCtrAddr = true;
                }
            }
        }

        Blob contractDataBlob;
        if (ethParams.isMember("data") && ethParams["data"].asString() != "")
        {
            auto strUnHexRes = strUnHex(ethParams["data"].asString().substr(2));
            if (!strUnHexRes)
            {
                return formatEthError(
                    ethERROR_DEFAULT, "contract_data is not in hex");
            }
            contractDataBlob = *strUnHexRes;
            if (contractDataBlob.size() == 0)
            {
                return RPC::invalid_field_error(jss::contract_data);
            }
        }
        else if (isCreation)
        {
            std::int64_t estimatedGas = TX_CREATE_GAS +
                (std::uint64_t)openViewTemp->fees().base.drops() /
                    openViewTemp->fees().gas_price;
            jvResult["result"] = toHexString(estimatedGas);
            return jvResult;
        }
        else if (!isCtrAddr)
        {
            std::int64_t estimatedGas = TX_GAS +
                (std::uint64_t)openViewTemp->fees().base.drops() /
                    openViewTemp->fees().gas_price;
            jvResult["result"] = toHexString(estimatedGas);
            return jvResult;
        }

        std::int64_t value = 0;
        if (ethParams.isMember("value"))
        {
            auto eValue = eth::u256(ethParams["value"].asString());
            value = uint64_t(eValue / weiPerDrop);
            // std::string valueStrHex =
            // ethParams["value"].asString().substr(2); value =
            // std::stoull(valueStrHex, 0, 16);
        }

        int64_t upperBound = 0;
        if (ethParams.isMember("gas"))
        {
            std::string gasStrHex = ethParams["gas"].asString().substr(2);
            upperBound = std::stoll(gasStrHex, 0, 16);
        }
        if (upperBound == 0 || upperBound == eth::Invalid256 ||
            upperBound > eth::c_maxGasEstimate)
            upperBound = eth::c_maxGasEstimate;

        int64_t lowerBound =
            Executive::baseGasRequired(isCreation, &contractDataBlob);

          STTx contractTx(
            ttCONTRACT,
            [&accountID,
             &contractAddrID,
             &value,
             &contractDataBlob,
             &isCreation](auto& obj) {
                obj.setAccountID(sfAccount, accountID);
                obj.setAccountID(sfContractAddress, contractAddrID);
                obj.setFieldVL(sfContractData, contractDataBlob);
                obj.setFieldU16(
                    sfContractOpType,
                    isCreation ? ContractCreation : MessageCall);
                if (value != 0)
                    obj.setFieldAmount(sfContractValue, ZXCAmount(value));
            });
        int64_t cap = upperBound;
        while (lowerBound + 1 < upperBound)
        {
            int64_t mid = (lowerBound + upperBound) / 2;
            contractTx.setFieldU32(sfGas, mid);
            auto tup = execute(context, contractTx, openViewTemp);
            auto execRet = std::get<0>(tup);

            if (execRet != tesSUCCESS)
                lowerBound = mid;
            else
                upperBound = mid;
        }
        if (upperBound == cap)
        {
            contractTx.setFieldU32(sfGas, upperBound);
            auto tup = execute(context, contractTx, openViewTemp);
            TER execRet;
            eth::owning_bytes_ref output, revertData;
            std::tie(execRet, output, revertData) =
                execute(context, contractTx, openViewTemp);

            std::string errMsg;
            if (execRet != tesSUCCESS)
                errMsg = output.toString();

            if (execRet != tesSUCCESS)
            {
                if (execRet == tefCONTRACT_REVERT_INSTRUCTION)
                {
                    return formatEthError(
                        ethERROR_REVERTED,
                        "0x" + strHex(revertData.takeBytes()),
                        revertMsg(errMsg));
                }
                return formatEthError(ethERROR_DEFAULT, errMsg);
            }
        }
        std::int64_t estimatedGas = upperBound +
            (std::uint64_t)openViewTemp->fees().base.drops() /
                openViewTemp->fees().gas_price;
        jvResult["result"] = toHexString(estimatedGas);
        return jvResult;
    }
    catch (std::exception const& e)
    {
        return formatEthError(ethERROR_DEFAULT, e.what());
        // TODO: Some sort of notification of failure.
        // jvResult["result"] = toHexString(eth::u256());
        // return jvResult;
    }
}

} // ripple
