cmake_minimum_required(VERSION 3.5)

add_executable(
    chainsqlWasm 
    main.cc)

include_directories(
    ".."
    "../libraries"
    "../libraries/boost/include"
)
target_link_libraries(chainsqlWasm PRIVATE wasmvm m3)
target_compile_features(chainsqlWasm PRIVATE cxx_std_17)

add_custom_command(
    TARGET chainsqlWasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:chainsqlWasm>
    ${CMAKE_BINARY_DIR}
)
