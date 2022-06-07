cmake_minimum_required(VERSION 3.5)

find_package(
    Boost
    1.70.0
)

if (Boost_FOUND)
    add_executable(chainsqlWasm main.cc)
    include_directories(
        ${CMAKE_SOURCE_DIR}/src/wasmvm
        ${CMAKE_SOURCE_DIR}/src/wasmvm/wasm3/source
        ${Boost_INCLUDE_DIRS}
    )
    target_link_libraries(chainsqlWasm PRIVATE wasmvm m3)
    target_compile_features(chainsqlWasm PRIVATE cxx_std_17)
endif()
