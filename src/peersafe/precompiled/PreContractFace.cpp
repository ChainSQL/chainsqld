#include <peersafe/precompiled/PreContractFace.h>
#include <peersafe/precompiled/Define.h>
#include <peersafe/precompiled/TableOpPrecompiled.h>
#include <peersafe/precompiled/ToolsPrecompiled.h>
#include <ripple/protocol/digest.h>
#include <peersafe/precompiled/Utils.h>

using namespace eth;
namespace ripple {



    PrecompiledOrigin::PrecompiledOrigin(
        std::string const &_name, int64_t const &_startingBlock /*= 0*/)
        : m_cost(PrecompiledRegistrar::pricer(_name)),
          m_execute(PrecompiledRegistrar::executor(_name)),
          m_startingBlock(_startingBlock)
    {}

    uint32_t
    PrecompiledDiyBase::getFuncSelector(
        std::string const& _functionName)
    {
        if (name2SelectCache_.count(_functionName))
        {
            return name2SelectCache_[_functionName];
        }
        auto selector = getFuncSelectorByFunctionName(_functionName);
        name2SelectCache_.insert(std::make_pair(_functionName, selector));
        return selector;
    }

    PreContractFace::PreContractFace()
    {
        // for (unsigned i = 1; i <= 4; ++i)
        //     genesisState[AccountID(i)] = Account(0, 1);
        // Setup default precompiled contracts as equal to genesis of Frontier.
        precompiled.insert(std::make_pair(AccountID(1), PrecompiledOrigin("ecrecover")));
        precompiled.insert(std::make_pair(AccountID(2), PrecompiledOrigin("sha256")));
        precompiled.insert(std::make_pair(AccountID(3), PrecompiledOrigin("ripemd160")));
        precompiled.insert(std::make_pair(AccountID(4), PrecompiledOrigin("identity")));

        precompiled.insert({AccountID(5), PrecompiledOrigin("modexp")});
        precompiled.insert({AccountID(6), PrecompiledOrigin("alt_bn128_G1_add")});
        precompiled.insert({AccountID(7), PrecompiledOrigin("alt_bn128_G1_mul")});
        precompiled.insert({AccountID(8), PrecompiledOrigin("alt_bn128_pairing_product")});
        // precompiled.insert({AccountID(9), PrecompiledOrigin("blake2_compression")});

        precompiled.insert(std::make_pair(SM3_ADDR, PrecompiledOrigin("sm3")));
        precompiled.insert(std::make_pair(EN_BASE58_ADDR, PrecompiledOrigin("enbase58")));
        precompiled.insert(std::make_pair(DE_BASE58_ADDR, PrecompiledOrigin("debase58")));

        //-----------------------------------------------------------------------------
        precompiledDiy.insert(std::make_pair(
            TABLE_OPERATION_ADDR, std::make_shared<TableOpPrecompiled>()));
        precompiledDiy.insert(std::make_pair(
            TOOLS_PRE_ADDR, std::make_shared<ToolsPrecompiled>()));
    }
}
