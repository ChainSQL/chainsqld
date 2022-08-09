#include <peersafe/precompiled/Utils.h>
#include <eth/tools/picosha2.h>

using namespace eth;
namespace ripple {
uint32_t
getParamFunc(bytesConstRef _param)
{
    auto funcBytes = _param.cropped(0, 4);
    uint32_t func = *((uint32_t*)(funcBytes.data()));

    return ((func & 0x000000FF) << 24) | ((func & 0x0000FF00) << 8) |
        ((func & 0x00FF0000) >> 8) | ((func & 0xFF000000) >> 24);
}

bytesConstRef
getParamData(bytesConstRef _param)
{
    return _param.cropped(4);
}

uint32_t
getFuncSelectorByFunctionName(std::string const& _functionName)
{
    evmc_uint256be hashRet;
    keccak(
        (uint8_t*)_functionName.data(),
        _functionName.length(),
        (uint8_t*)hashRet.bytes);
    bytesConstRef vec{hashRet.bytes, 32};
    uint32_t func = *(uint32_t*)(vec.cropped(0, 4).data());
    uint32_t selector = ((func & 0x000000FF) << 24) |
        ((func & 0x0000FF00) << 8) | ((func & 0x00FF0000) >> 8) |
        ((func & 0xFF000000) >> 24);
    return selector;
}

uint256
eth_sha256(Slice const& slice)
{
    uint256 ret;
    picosha2::hash256(slice.begin(), slice.end(), ret.data(), ret.data() + 32);
    return ret;
}

}  // namespace ripple