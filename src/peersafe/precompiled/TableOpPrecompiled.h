#ifndef PRECOMPILED_TABLE_OP_H_INCLUDE
#define PRECOMPILED_TABLE_OP_H_INCLUDE

#include <peersafe/precompiled/PreContractFace.h>

namespace ripple {
class TableOpPrecompiled : public PrecompiledDiyBase
{
public:
    TableOpPrecompiled();
    std::string toString() override;

    std::tuple<TER, eth::bytes, int64_t>
    execute(
        SleOps& _s,
        eth::bytesConstRef _in,
        AccountID const& caller = AccountID(),
        AccountID const& origin = AccountID()) override;
};
}  // namespace ripple

#endif