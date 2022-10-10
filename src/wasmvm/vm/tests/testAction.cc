#include <tuple>
#include <vm/action.h>
#include <vm/chainsqlWasmVm.h>

#include <contract/math/math.wasm.h>
#include <chainsql/core/datestream.h>

int main(int argc, char **argv) {
    std::tuple<int, int> args = std::make_tuple(100,200);
    std::string payload;
    payload.resize(2*sizeof(int));
    chainsql::datastream<char *> ds = chainsql::datastream<char *>(payload.data(), payload.size());
    chainsql::action action(chainsql::name("math"), chainsql::name("add"), payload);

    // initialize a vm 
    chainsql::chainsqlWasmVm vm(1024);
    // load wasm code and return a module
    wasm3::module mod = vm.loadWasm(math_wasm, math_wasm_len);

    // load import functions for module
    chainsql::importGlobalFunctions global;
    global.load(mod);

    int ret = vm.apply<int>(chainsql::actionCallBack<int>(action), mod); 
    return 0;
}