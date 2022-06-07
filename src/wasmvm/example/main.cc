#include <iostream>
#include <string>
#include <tuple>

#include "vm/chainsqlWasmVm.h"
#include "common/name.h"
#include "common/datastream.h"
#include "vm/actionCallback.h"

#include "contract/math/math.wasm.h"

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
        std::cout << "chainsql_assert "
               << "test = " << test
               << ", msg = " << std::string((const char *)msg) << std::endl;
    };
    mod.link("*", "chainsql_assert",  &assert_fun);

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
    catch (std::runtime_error &e)
    {
        std::cerr << "WASM3 error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
