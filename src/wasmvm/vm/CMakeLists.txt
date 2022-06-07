set(sources
    "./impl/chainsqlWasmVm.cc"
    "./impl/action.cc"
)

set(headers
    "./chainsqlWasmVm.h"
    "./actionCallback.h"
    "./action.h"
)

add_library(wasmvm STATIC ${sources} ${headers})

include_directories(
    ${CMAKE_SOURCE_DIR}/src/wasmvm
)

target_include_directories(
    wasmvm 
    PUBLIC 
    ${CMAKE_SOURCE_DIR}/src/wasmvm
    ${CMAKE_SOURCE_DIR}/src/wasmvm/wasm3/source
)

target_compile_features(wasmvm PRIVATE cxx_std_17)
