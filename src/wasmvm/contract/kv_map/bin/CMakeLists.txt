cmake_minimum_required(VERSION 3.5)
project(kv_map)

set(SOURCES
    main.cc
    storage/impl/exports.cc
)

set(HEADERS
    #exports.h
)

add_executable(
    kv_map 
    ${SOURCES}
    ${HEADERS})

include_directories(
    "./storage"
    "../../../libraries"
    "../../../libraries/meta_refl/include"
    "../../../libraries/boost/include"
)
target_compile_features(kv_map PRIVATE cxx_std_17)
