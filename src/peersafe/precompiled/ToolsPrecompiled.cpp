#include <ripple/basics/StringUtilities.h>
#include <peersafe/precompiled/ABI.h>
#include <peersafe/precompiled/ToolsPrecompiled.h>
#include <peersafe/precompiled/Utils.h>

namespace ripple {
const char* const VERIFY_SIGNATURE_STR = "verify(string,string,string)";

ToolsPrecompiled::ToolsPrecompiled()
{
    name2Selector_[VERIFY_SIGNATURE_STR] =
        getFuncSelector(VERIFY_SIGNATURE_STR);
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
        if (func == name2Selector_[VERIFY_SIGNATURE_STR])
        {
            std::string payload, signature, publicKey;
            uint256 retValue;
            abi.abiOut(data, payload, signature,publicKey);
            auto payloadBytes = strUnHex(payload);
            auto sigBytes = strUnHex(signature);
            auto pubKey = PublicKey(makeSlice(
                decodeBase58Token(publicKey, TokenType::AccountPublic)));
            if (!payloadBytes || !sigBytes || pubKey.empty())
                ret.push_back(0);
            else
            {
                //Cannot use bool,will crash when evmcResult.release called
                retValue = verify(
                    pubKey,
                    makeSlice(*payloadBytes),
                    makeSlice(*sigBytes),
                    true);
                ret = Blob(retValue.begin(), retValue.end());
            }            
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