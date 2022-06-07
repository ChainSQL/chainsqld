cmake_minimum_required(VERSION 3.5)

find_package(
    Boost
    1.70.0
)

if (Boost_FOUND)
    add_executable(
        chainsqlWasm 
        main.cc
        ${CMAKE_SOURCE_DIR}/src/wasmvm/vm/impl/action.cc
        ${CMAKE_SOURCE_DIR}/src/wasmvm/vm/impl/chainsqlWasmVm.cc
    )
    include_directories(
        ${CMAKE_SOURCE_DIR}/src/
        ${CMAKE_SOURCE_DIR}/src/wasmvm/wasm3/source
        ${CMAKE_SOURCE_DIR}/src/wasmvm/wasm3/platforms/cpp/wasm3_cpp/include
        ${Boost_INCLUDE_DIRS}
    )
    
    target_link_libraries(chainsqlWasm PRIVATE wasm3_cpp m3)
    target_compile_features(chainsqlWasm PRIVATE cxx_std_17)
endif()
