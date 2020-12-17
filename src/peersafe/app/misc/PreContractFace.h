#ifndef __PRE_CONTRACT_FACE__
#define __PRE_CONTRACT_FACE__

#include <peersafe/app/misc/PreContractRegister.h>

namespace ripple {

    class PrecompiledContract
    {
    public:
        PrecompiledContract(std::string const &_name, int64_t const &_startingBlock = 0);

        int64_t cost(
            eth::bytesConstRef _in, int64_t const &_blockNumber) const
        {
            return m_cost(_in, _blockNumber);
        }
        std::pair<bool, eth::bytes> execute(eth::bytesConstRef _in) const { return m_execute(_in); }

        int64_t const &startingBlock() const { return m_startingBlock; }

    private:
        PrecompiledPricer m_cost;
        PrecompiledExecutor m_execute;
        int64_t m_startingBlock = 0;
    };

    class PreContractFace
    {
    public:
        PreContractFace();

        bool isPrecompiled(AccountID const &_a, int64_t const &_blockNumber) const
        {
            return precompiled.count(_a) != 0 && _blockNumber >= precompiled.at(_a).startingBlock();
        }
        int64_t costOfPrecompiled(
            AccountID const &_a, eth::bytesConstRef _in, int64_t const &_blockNumber) const
        {
            return precompiled.at(_a).cost(_in, _blockNumber);
        }
        std::pair<bool, eth::bytes> executePrecompiled(
            AccountID const &_a, eth::bytesConstRef _in, int64_t const &) const
        {
            return precompiled.at(_a).execute(_in);
        }
    
    private:
        /// Precompiled contracts as specified in the chain params.
        std::unordered_map<AccountID, PrecompiledContract> precompiled;
    };
}         

#endif