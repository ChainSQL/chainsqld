#include "native/chainsql/intrinsics.hpp"
#include "chainsqlib/capi/system.h"
#include "vm/chainsqlWasmVm.h"

namespace chainsql {

chainsqlWasmVm::chainsqlWasmVm(size_t stack_size_bytes) throw()
: env_()
, runtime_(env_.new_runtime(stack_size_bytes)) {

}

chainsqlWasmVm::~chainsqlWasmVm() {

}

wasm3::module chainsqlWasmVm::loadWasm(const uint8_t *data, size_t size) throw() {
    wasm3::module mod = env_.parse_module(data, size);
    runtime_.load(mod);
    return mod;
}

}
