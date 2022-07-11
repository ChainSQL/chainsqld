#ifndef PRECOMPILED_UTILS_H_INCLUDE
#define PRECOMPILED_UTILS_H_INCLUDE

#include <eth/evmc/include/evmc/evmc.h>
#include <eth/vm/Common.h>
#include <eth/vm/utils/keccak.h>
#include <peersafe/precompiled/picosha2.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>

using namespace eth;
namespace ripple {
uint32_t
getParamFunc(bytesConstRef _param);

bytesConstRef
getParamData(bytesConstRef _param);

uint32_t
getFuncSelectorByFunctionName(std::string const& _functionName);

template <class Iterator>
std::string
toHex(Iterator _it, Iterator _end, std::string const& _prefix)
{
    typedef std::iterator_traits<Iterator> traits;
    static_assert(
        sizeof(typename traits::value_type) == 1,
        "toHex needs byte-sized element type");

    static char const* hexdigits = "0123456789abcdef";
    size_t off = _prefix.size();
    std::string hex(std::distance(_it, _end) * 2 + off, '0');
    hex.replace(0, off, _prefix);
    for (; _it != _end; _it++)
    {
        hex[off++] = hexdigits[(*_it >> 4) & 0x0f];
        hex[off++] = hexdigits[*_it & 0x0f];
    }
    return hex;
}

template <class T>
std::string
toHex(T const& _data)
{
    return toHex(_data.begin(), _data.end(), "");
}

uint256
eth_sha256(Slice const& slice);

}  // namespace ripple
#endif