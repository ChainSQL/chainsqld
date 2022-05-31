#pragma once

#include <functional>

#include "vm/actionCallback.h"

#include "wasm3_cpp.h"

namespace chainsql {

class chainsqlWasmVm
{
private:
    wasm3::environment env_;
    wasm3::runtime runtime_;
public:
    chainsqlWasmVm(size_t stack_size_bytes) throw();
    ~chainsqlWasmVm();
    
    wasm3::module loadWasm(const uint8_t *data, size_t size) throw();

    template<typename Ret = void, typename ... Args>
    Ret invoke(const char* function, Args ...args) {
        wasm3::function fn = runtime_.find_function(function);
        return fn.call<Ret>(args...);
    }

    template<typename T>
    T apply(actionCallback<T> *cb)
    {
        invoke(
            "apply", 
            cb->Action().code().value,
            cb->Action().code().value,
            cb->Action().function().value);

        return cb->result();
    }
};
} // namesapce chainsql

