#ifndef PRECOMPILED_REGISTER_H_INCLUDE
#define PRECOMPILED_REGISTER_H_INCLUDE

#include <unordered_map>
#include <functional>
#include <eth/vm/Common.h>
#include <eth/vm/utils/keccak.h>

/*
#define DEV_SIMPLE_EXCEPTION(X)       \
    struct X : virtual eth::Exception \
    {                                 \
    }
*/

namespace ripple
{
// struct ChainOperationParams;

using PrecompiledExecutor = std::function<std::pair<bool, eth::bytes>(eth::bytesConstRef _in)>;
using PrecompiledPricer = std::function<int64_t(
    eth::bytesConstRef _in, int64_t const& _blockNumber)>;

DEV_SIMPLE_EXCEPTION(ExecutorNotFound);
DEV_SIMPLE_EXCEPTION(PricerNotFound);

class PrecompiledRegistrar
{
public:
    /// Get the executor object for @a _name function or @throw ExecutorNotFound if not found.
    static PrecompiledExecutor const& executor(std::string const& _name);

    /// Get the price calculator object for @a _name function or @throw PricerNotFound if not found.
    static PrecompiledPricer const& pricer(std::string const& _name);

    /// Register an executor. In general just use ETH_REGISTER_PRECOMPILED.
    static PrecompiledExecutor registerExecutor(std::string const& _name, PrecompiledExecutor const& _exec) { return (get()->m_execs[_name] = _exec); }
    /// Unregister an executor. Shouldn't generally be necessary.
    static void unregisterExecutor(std::string const& _name) { get()->m_execs.erase(_name); }

    /// Register a pricer. In general just use ETH_REGISTER_PRECOMPILED_PRICER.
    static PrecompiledPricer registerPricer(std::string const& _name, PrecompiledPricer const& _exec) { return (get()->m_pricers[_name] = _exec); }
    /// Unregister a pricer. Shouldn't generally be necessary.
    static void unregisterPricer(std::string const& _name) { get()->m_pricers.erase(_name); }

private:
    static PrecompiledRegistrar* get() { if (!s_this) s_this = new PrecompiledRegistrar; return s_this; }

    std::unordered_map<std::string, PrecompiledExecutor> m_execs;
    std::unordered_map<std::string, PrecompiledPricer> m_pricers;
    static PrecompiledRegistrar* s_this;
};

// TODO: unregister on unload with a static object.
#define ETH_REGISTER_PRECOMPILED(Name)                                             \
    static std::pair<bool, eth::bytes>                                             \
        __eth_registerPrecompiledFunction##Name(eth::bytesConstRef _in);           \
    static PrecompiledExecutor __eth_registerPrecompiledFactory ## Name =          \
        PrecompiledRegistrar::registerExecutor(                                    \
            #Name, &__eth_registerPrecompiledFunction ## Name);                    \
    static std::pair<bool, eth::bytes> __eth_registerPrecompiledFunction ## Name

#define ETH_REGISTER_PRECOMPILED_PRICER(Name)                                      \
    static int64_t __eth_registerPricerFunction##Name(                             \
        eth::bytesConstRef _in, int64_t const& _blockNumber);                      \
    static PrecompiledPricer __eth_registerPricerFactory##Name =                   \
        PrecompiledRegistrar::registerPricer(                                      \
            #Name, &__eth_registerPricerFunction##Name);                           \
    static int64_t __eth_registerPricerFunction##Name
}

#endif
