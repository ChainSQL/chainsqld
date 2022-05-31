#pragma once

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/include/std_tuple.hpp>
#include <boost/mp11/tuple.hpp>

#include <chainsqlib/core/name.h>
#include <chainsqlib/core/datastream.h>
#include <chainsqlib/contracts/action.h>

#define WASM_EXPORT __attribute__((used)) __attribute__((visibility ("default")))

namespace chainsql {

template <typename Ret, typename T, typename... Args>
bool execute_action(name self, name code, Ret (T::*func)(Args...)) {
    size_t size = action_data_size();

    // using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void *buffer = nullptr;
    if (size > 0)
    {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    std::tuple<std::decay_t<Args>...> args;
    datastream<const char *> ds((char *)buffer, size);
    ds >> args;

    T inst(self, code, ds);
    Ret ret;
    auto f2 = [&](auto... a)
    {
        ret = ((&inst)->*func)(a...);
    };

    boost::mp11::tuple_apply(f2, args);
    if (max_stack_buffer_size < size)
    {
        free(buffer);
    }
    set_action_return_value(&ret, sizeof(Ret));
    return true;
}

template <typename T, typename... Args>
bool execute_action(name self, name code, void (T::*func)(Args...)) {
    size_t size = action_data_size();

    // using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void *buffer = nullptr;
    if (size > 0)
    {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    std::tuple<std::decay_t<Args>...> args;
    datastream<const char *> ds((char *)buffer, size);
    ds >> args;

    T inst(self, code, ds);
    auto f2 = [&](auto... a)
    {
        ((&inst)->*func)(a...);
    };

    boost::mp11::tuple_apply(f2, args);
    if (max_stack_buffer_size < size)
    {
        free(buffer);
    }

    return true;
}

}

// Helper macro for CHAINSQL_DISPATCH_INTERNAL
#define CHAINSQL_DISPATCH_INTERNAL(r, OP, elem)                                        \
    case chainsql::name(BOOST_PP_STRINGIZE(elem)).value:                               \
        chainsql::execute_action(chainsql::name(receiver), chainsql::name(code), &OP::elem); \
        break;

// Helper macro for CHAINSQL_DISPATCH
#define CHAINSQL_DISPATCH_HELPER(TYPE, MEMBERS) \
    BOOST_PP_SEQ_FOR_EACH(CHAINSQL_DISPATCH_INTERNAL, TYPE, MEMBERS)
    

/**
 * Convenient macro to create contract apply handler
 *
 * @ingroup dispatcher
 * @note To be able to use this macro, the contract needs to be derived from chainsql::contract
 * @param TYPE - The class name of the contract
 * @param MEMBERS - The sequence of available actions supported by this contract
 *
 * Example:
 * @code
 * CHAINSQL_DISPATCH( chainsql::contract, (setpriv)(setalimits)(setglimits)(setprods)(reqauth) )
 * @endcode
 */
#define CHAINSQL_DISPATCH( TYPE, MEMBERS ) \
extern "C" { \
   void WASM_EXPORT apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      if( code == receiver ) { \
         switch( action ) { \
            CHAINSQL_DISPATCH_HELPER( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: chainsql_exit(0); */ \
      } \
   } \
}