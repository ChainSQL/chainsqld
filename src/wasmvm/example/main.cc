#include <iostream>
#include <string>
#include <tuple>

#include "chainsqlWasmVm.h"
#include "chainsqlib/core/name.h"
#include "chainsqlib/core/datastream.h"
#include "native/chainsql/intrinsics.hpp"
#include "vm/actionCallback.h"

#include "contract/math/math.wasm.h"
#include "contract/hello/hello.wasm.h"

void execute_math_contract()
{
    std::cout << "execute math contract, ";
    // serilize args
    std::tuple<int, int> args = std::make_tuple(100, 200);
    std::string payload;
    payload.resize(2 * sizeof(int));
    chainsql::datastream<char *> ds = chainsql::datastream<char *>(payload.data(), payload.size());
    ds << args;

    // initailize an action
    chainsql::action action(chainsql::name("math"), chainsql::name("add"), payload);
    chainsql::chainsqlWasmVm vm(5120);
    wasm3::module mod = vm.loadWasm(math_wasm, math_wasm_len);

    // imports global functions into module
    chainsql::native::intrinsics::get().set_intrinsic<chainsql::native::intrinsics::chainsql_assert>(
        [&](int32_t test, const void *msg)
        {
            std::cout << "chainsql_assert "
                      << "test = " << test
                      << ", msg = " << std::string((const char *)msg) << std::endl;
        });
    mod.link("*", "chainsql_assert", chainsql_assert);

    // import functions to a specified action
    chainsql::actionCallback<int> cb(action, mod);
    // invoke an action
    int ret = vm.apply<int>(&cb);
    std::cout << "result = " << ret << std::endl;
}

void execute_hello_contract() 
{
    std::cout << "execute hello contract, output: " << std::endl;
    // serilize args
    void* data = (void*)"hello,peersafe";
    int64_t p = (int64_t)data;
    std::tuple<int64_t, int32_t> args = std::make_tuple(p, strlen("hello,peersafe"));
    std::string payload;
    payload.resize(sizeof(int64_t)+sizeof(int32_t));
    chainsql::datastream<char *> ds = chainsql::datastream<char *>(payload.data(), payload.size());
    ds << args;

    // initailize an action
    chainsql::action action(chainsql::name("hello"), chainsql::name("hi"), payload);
    chainsql::chainsqlWasmVm vm(5120);
    wasm3::module mod = vm.loadWasm(hello_wasm, hello_wasm_len);

    // imports global functions into module
    chainsql::native::intrinsics::get().set_intrinsic<chainsql::native::intrinsics::chainsql_assert>(
        [&](int32_t test, const void *msg)
        {
            int64_t p = (int64_t)msg;
            std::cout << "chainsql_assert "
                      << "test = " << test
                      << ", msg = " << std::string((const char *)msg) << std::endl;
        });
    mod.link("*", "chainsql_assert", chainsql_assert);

    chainsql::native::intrinsics::get().set_intrinsic<chainsql::native::intrinsics::chainsql_memcpy>(
        [&](void* dst, uint64_t src, uint32_t len)
        {
            void* src_p = (void*)src;
            std::memcpy(dst, src_p, len);
        });
    mod.link("*", "chainsql_memcpy", chainsql_memcpy);

    // import functions to a specified action
    chainsql::actionCallback<int32_t> cb(action, mod);
    // invoke an action
    int32_t ret = vm.apply<int32_t>(&cb);
    std::cout << "result = " << ret << std::endl;
}

int main(int argc, char** argv) {
    try {
        execute_math_contract();
        execute_hello_contract();
    }
    catch (std::runtime_error &e)
    {
        std::cerr << "WASM3 error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
