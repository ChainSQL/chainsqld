

#include <ripple/ledger/OpenView.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/STTx.h>
#include <cctype>
#include <chrono>
#include <eth/api/utils/Helpers.h>
#include <eth/api/utils/KeyPair.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <eth/api/utils/TransactionSkeleton.h>

namespace ripple {

std::string
ethAddrChecksum(std::string addr)
{
    std::string addrTemp = (addr.substr(0, 2) == "0x") ? addr.substr(2) : addr;
    if (addrTemp.length() != 40)
        return std::string("");

    transform(addrTemp.begin(), addrTemp.end(), addrTemp.begin(), ::tolower);

    Blob addrSha3;
    addrSha3.resize(32);
    eth::sha3(
        (const uint8_t*)addrTemp.data(), addrTemp.size(), addrSha3.data());
    std::string addrSha3Str = strHex(addrSha3.begin(), addrSha3.end());
    std::string ret;

    for (int i = 0; i < addrTemp.length(); i++)
    {
        if (charUnHex(addrSha3Str[i]) >= 8)
        {
            ret += toupper(addrTemp[i]);
        }
        else
        {
            ret += addrTemp[i];
        }
    }

    return ret;
}

Json::Value
formatEthError(int code)
{
    Json::Value jvResult;
    Json::Value jvError;
    jvError["code"] = code;
    jvError["message"] = RPC::get_error_msg(error_code_eth(code));
    jvError["data"] = {};
    jvResult["error"] = jvError;

    return jvResult;
}

Json::Value
formatEthError(int code, std::string const& msg)
{
    Json::Value jvResult;
    Json::Value jvError;
    jvError["code"] = code;
    jvError["message"] = msg;
    jvError["data"] = {};
    jvResult["error"] = jvError;

    return jvResult;
}

Json::Value
formatEthError(int code, error_code_i rpcCode)
{
    return formatEthError(code, RPC::get_error_info(rpcCode).message.c_str());
}

void
ethLdgIndex2chainsql(Json::Value& params, std::string ledgerIndexStr)
{
    if (ledgerIndexStr == "latest")
    {
        params[jss::ledger_index] = "validated";
    }
    else if (ledgerIndexStr == "pending")
    {
        params[jss::ledger_index] = "closed";
    }
    else if (ledgerIndexStr == "earliest")
    {
        params[jss::ledger_index] = 1;
    }
    else
    {
        ledgerIndexStr = ledgerIndexStr.substr(2);
        params[jss::ledger_index] = (int64_t)std::stoll(ledgerIndexStr, 0, 16);
    }
}

uint64_t
getChainID(std::shared_ptr<OpenView const> const& ledger)
{
    std::shared_ptr<SLE const> sleChainID = ledger->read(keylet::chainId());
    uint256 chainID = sleChainID->getFieldH256(sfChainId);
    std::string chainIDStr = to_string(chainID).substr(60);
    uint64_t realChainID = (uint64_t)std::stoll(chainIDStr, 0, 16);
    return realChainID;
}

eth::h160
addressFromSecret(std::string const& sSecret)
{
    eth::h160 ret;
    auto secret = strUnHex(sSecret.substr(2));
    if (secret)
    {
        eth::Secret secret(sSecret);
        eth::KeyPair keyPair(secret);
        ret = keyPair.address();
    }
    
    return ret;
}

TransactionSkeleton
toTransactionSkeleton(Json::Value const& _json)
{
    using namespace eth;
    TransactionSkeleton ret;
    if (!_json.isObject())
        return ret;

    if (!_json["from"].isNull())
        ret.from = h160(_json["from"].asString());
    if (!_json["to"].isNull() && !_json["to"].asString().empty())
        ret.to = h160(_json["to"].asString());
    else
        ret.creation = true;

    if (!_json["value"].isNull())
        ret.value = u256(_json["value"].asString());

    if (!_json["gas"].isNull())
        ret.gas = u256(_json["gas"].asString());

    if (!_json["gasPrice"].isNull())
        ret.gasPrice = u256(_json["gasPrice"].asString());

    if (!_json["data"].isNull())
        ret.data = strUnHex(_json["data"].asString()).get();

    if (!_json["nonce"].isNull())
        ret.nonce = u256(_json["nonce"].asString());
    return ret;
}

}  // namespace ripple
