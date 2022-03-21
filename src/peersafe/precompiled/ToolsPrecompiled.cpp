#include <ripple/basics/StringUtilities.h>
#include <peersafe/precompiled/ABI.h>
#include <peersafe/precompiled/ToolsPrecompiled.h>
#include <peersafe/precompiled/Utils.h>

namespace ripple {
const char* const VERIFY_SIGNATURE_STR = "verify(string,string,string)";
const char* const PUBLIC_TO_ADDRESS = "publicToAddress(string)";

ToolsPrecompiled::ToolsPrecompiled()
{
    name2Selector_[VERIFY_SIGNATURE_STR] =
        getFuncSelector(VERIFY_SIGNATURE_STR);
    name2Selector_[PUBLIC_TO_ADDRESS] = 
        getFuncSelector(PUBLIC_TO_ADDRESS);
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
        int64_t ter(0), runGas(0);;
        Blob ret;
        uint256 retValue(0);
        if (func == name2Selector_[VERIFY_SIGNATURE_STR])
        {
            std::string payload, signature, publicKey;
            abi.abiOut(data, payload, signature,publicKey);
            //auto payloadBytes = strUnHex(payload);
            auto sigBytes = strUnHex(signature);
            auto pubKey = PublicKey(makeSlice(
                decodeBase58Token(publicKey, TokenType::AccountPublic)));
            if (payload.empty() || !sigBytes || pubKey.empty())
                retValue = uint256(0);
            else
            {
                //Cannot use bool,will crash when evmcResult.release called
                retValue = verify(
                    pubKey,
                    makeSlice(payload),
                    makeSlice(*sigBytes),
                    true);
            }          
        }
        if (func == name2Selector_[PUBLIC_TO_ADDRESS])
        {
            std::string publicKey;
            abi.abiOut(data, publicKey);
            auto pubKey = PublicKey(makeSlice(
                decodeBase58Token(publicKey, TokenType::AccountPublic)));
            if (pubKey.empty())
                retValue = uint256(0);
            else
            {
                auto accID = calcAccountID(pubKey);
                std::copy(accID.begin(), accID.end(), retValue.begin() + (retValue.size() - accID.size()));
            }
        }
        ret = Blob(retValue.begin(), retValue.end());

        return std::make_tuple(
            TER::fromInt(ter), Blob(ret.begin(), ret.end()), runGas);
    }
    else
    {
        return std::make_tuple(tesSUCCESS, strCopy(""), 0);
    }
}
}  // namespace ripple