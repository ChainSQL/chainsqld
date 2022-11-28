#ifndef PRECOMPILED_UTILS_H_INCLUDE
#define PRECOMPILED_UTILS_H_INCLUDE

#include <eth/evmc/include/evmc/evmc.h>
#include <eth/vm/Common.h>
#include <eth/vm/utils/keccak.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/Blob.h>

using namespace eth;
namespace ripple {
uint32_t
getParamFunc(bytesConstRef _param);

bytesConstRef
getParamData(bytesConstRef _param);

uint32_t
getFuncSelectorByFunctionName(std::string const& _functionName);

uint256
eth_sha256(Slice const& slice);

}  // namespace ripple
#endif