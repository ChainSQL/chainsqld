#ifndef __PRE_CONTRACT_FACE__
#define __PRE_CONTRACT_FACE__

#include <peersafe/precompiled/PreContractRegister.h>
#include <peersafe/app/misc/SleOps.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/TER.h>

namespace ripple {

    class PrecompiledOrigin
    {
    public:
        PrecompiledOrigin(std::string const &_name, int64_t const &_startingBlock = 0);

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

    class PrecompiledDiyBase
    {
    public:
        virtual ~PrecompiledDiyBase()
        {
        }
        virtual std::string toString()
        {
            return "";
        }

        virtual std::tuple<TER, eth::bytes, int64_t>
        execute(
            SleOps& _s,
            eth::bytesConstRef _in,
            AccountID const& caller = AccountID(),
            AccountID const& msgSender = AccountID()) = 0;

        virtual uint32_t
        getFuncSelector(std::string const& _functionName);
    protected:
        std::map<std::string, uint32_t> name2Selector_;
        std::unordered_map<std::string, uint32_t> name2SelectCache_;
    };

    class PreContractFace
    {
    public:
        PreContractFace();

        bool isPrecompiledOrigin(AccountID const &_a, int64_t const &_blockNumber) const
        {
            return precompiled.count(_a) != 0 && _blockNumber >= precompiled.at(_a).startingBlock();
        }

        bool isPrecompiledDiy(AccountID const& _a) const
        {
            return precompiledDiy.count(_a) != 0;
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

        std::tuple<TER, eth::bytes,int64_t>
        executePreDiy(
            SleOps& _s,
            AccountID const& _a,
            eth::bytesConstRef _in,
            AccountID const& caller = AccountID(),
            AccountID const& origin = AccountID())const
        {
            return precompiledDiy.at(_a)->execute(_s, _in, caller, origin);
        }
    
    private:
        /// Precompiled contracts as specified in the chain params.
        std::unordered_map<AccountID, PrecompiledOrigin> precompiled;
        std::unordered_map<AccountID, std::shared_ptr<PrecompiledDiyBase>> precompiledDiy;
    };
}         

#endif