set(sources
    "./impl/chainsqlWasmVm.cc"
    "./impl/action.cc"
    "../libraries/native/intrinsics.cc"
)

set(headers
    "./chainsqlWasmVm.h"
    "./actionCallback.h"
    "./action.h"
)

add_library(wasmvm STATIC ${sources} ${headers})

include_directories(
    ".."
    "../wasm3/source"
    "../wasm3/platforms/cpp/wasm3_cpp/include"
    "../libraries"
    #"../libraries/boost/include"
)

target_include_directories(
    wasmvm 
    PUBLIC 
    "./"
    "../wasm3/source"
    "../wasm3/platforms/cpp/wasm3_cpp/include"
    #"../libraries"
    #"../libraries/boost/include"
    )

target_compile_features(wasmvm PRIVATE cxx_std_17)
