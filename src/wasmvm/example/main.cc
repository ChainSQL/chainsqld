#include <iostream>
#include <string>
#include <tuple>
#include <exception>

#include "wasmvm/vm/chainsqlWasmVm.h"
#include "wasmvm/vm/actionCallback.h"
#include "wasmvm/common/name.h"
#include "wasmvm/common/datastream.h"

#include "wasmvm/contract/math/math.wasm.h"

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
    std::function<void(int32_t, const void *)> assert_fun = [&](int32_t test, const void *msg) {
        std::string message((const char*)msg);
        throw std::runtime_error(message);
    };
    mod.link("*", "chainsql_assert",  &assert_fun);

    std::function<void(int64_t)> contract_name_fn = [&](int64_t name) {
    };
    mod.link_optional("*", "chainsql_set_contract_name", &contract_name_fn);

    std::function<void(int32_t, int64_t)> assert_code_fn = [&](int32_t, int64_t) {
    };
    mod.link_optional("*", "chainsql_assert_code", &assert_code_fn);

    // import functions to a specified action
    chainsql::actionCallback<int> cb(action, mod);
    // invoke an action
    int ret = vm.apply<int>(&cb);
    std::cout << "result = " << ret << std::endl;
}

int main(int argc, char** argv) {
    try {
        execute_math_contract();
    }
    catch (wasm3::error &e)
    {
        std::cerr << "WASM3 error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
