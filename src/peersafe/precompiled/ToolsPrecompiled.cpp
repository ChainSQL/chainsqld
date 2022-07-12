#include <ripple/basics/StringUtilities.h>
#include <peersafe/precompiled/ABI.h>
#include <peersafe/precompiled/ToolsPrecompiled.h>
#include <peersafe/precompiled/Utils.h>

namespace ripple {

const char* const VERIFY_SIGNATURE_STR = "verify(string,string,string)";
const char* const PUBLIC_TO_ADDRESS = "publicToAddress(string)";
const char* const STRING_CONCAT = "stringConcat(string[])";
const char* const SHA256_COMMON = "eth_sha256(bytes)";
const char* const RIPEMD160 = "eth_ripemd160(bytes)";

ToolsPrecompiled::ToolsPrecompiled()
{
    name2Selector_[VERIFY_SIGNATURE_STR] =
        getFuncSelector(VERIFY_SIGNATURE_STR);
    name2Selector_[PUBLIC_TO_ADDRESS] = getFuncSelector(PUBLIC_TO_ADDRESS);
    name2Selector_[STRING_CONCAT] = getFuncSelector(STRING_CONCAT);
    name2Selector_[SHA256_COMMON] = getFuncSelector(SHA256_COMMON);
    name2Selector_[RIPEMD160] = getFuncSelector(RIPEMD160);
}

std::string
ToolsPrecompiled::toString()
{
    return "Tools";
}

std::tuple<TER, eth::bytes, int64_t>
ToolsPrecompiled::execute(
    SleOps& _s,
    eth::bytesConstRef _in,
    AccountID const& caller,
    AccountID const& origin)
{
    uint32_t func = getParamFunc(_in);
    bytesConstRef data = getParamData(_in);
    ContractABI abi;
    if (func)
    {
        int64_t ter(0), runGas(0);
        Blob ret;
        if (func == name2Selector_[VERIFY_SIGNATURE_STR])
        {
            uint256 retValue(0);
            std::string payload, signature, publicKey;
            abi.abiOut(data, payload, signature, publicKey);
            // auto payloadBytes = strUnHex(payload);
            auto sigBytes = strUnHex(signature);
            auto pubKey = PublicKey(makeSlice(
                decodeBase58Token(publicKey, TokenType::AccountPublic)));
            if (payload.empty() || !sigBytes || pubKey.empty())
                retValue = uint256(0);
            else
            {
                // Cannot use bool,will crash when evmcResult.release called
                retValue = verify(
                    pubKey, makeSlice(payload), makeSlice(*sigBytes), true);
            }
            ret = Blob(retValue.begin(), retValue.end());
        }
        else if (func == name2Selector_[PUBLIC_TO_ADDRESS])
        {
            uint256 retValue(0);
            std::string publicKey;
            abi.abiOut(data, publicKey);
            auto pubKey = PublicKey(makeSlice(
                decodeBase58Token(publicKey, TokenType::AccountPublic)));
            if (pubKey.empty())
                retValue = uint256(0);
            else
            {
                auto accID = calcAccountID(pubKey);
                std::copy(
                    accID.begin(),
                    accID.end(),
                    retValue.begin() + (retValue.size() - accID.size()));
            }
            ret = Blob(retValue.begin(), retValue.end());
        }
        else if (func == name2Selector_[STRING_CONCAT])
        {
            std::vector<std::string> vString;
            abi.abiOut(data, vString);
            for (auto i = 1; i < vString.size(); i++)
            {
                vString[0].append(vString[i]);
            }
            ret = abi.abiIn("", vString[0]);
        }
        else if (func == name2Selector_[SHA256_COMMON])
        {
            std::string param;
            abi.abiOut(data, param);
            uint256 shaRes = eth_sha256(makeSlice(param));
            ret = Blob(shaRes.begin(), shaRes.end());
        }
        else if (func == name2Selector_[RIPEMD160])
        {
            std::string param;
            abi.abiOut(data, param);
            ret = eth_ripemd160(makeSlice(param));
        }

        return std::make_tuple(
            TER::fromInt(ter), Blob(ret.begin(), ret.end()), runGas);
    }
    else
    {
        return std::make_tuple(tesSUCCESS, strCopy(""), 0);
    }
}

}  // namespace ripple