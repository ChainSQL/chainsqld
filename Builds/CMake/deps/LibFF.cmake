# Aleth: Ethereum C++ client, tools and libraries.
# Copyright 2017-2019 Aleth Authors.
# Licensed under the GNU General Public License, Version 3.
include(MPIR)

#set(prefix "${CMAKE_BINARY_DIR}/deps")
set(libff_library "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ff${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(libff_inlcude_dir "${nih_cache_path}/include/libff")

ExternalProject_Add(libff
    PREFIX ${nih_cache_path}
    GIT_REPOSITORY https://github.com/scipr-lab/libff.git
    GIT_TAG 03b719a7c81757071f99fc60be1f7f7694e51390
    CMAKE_ARGS
        $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DGMP_INCLUDE_DIR=${MPIR_INCLUDE_DIR}
        -DGMP_LIBRARY=${MPIR_LIBRARY}
        -DCURVE=ALT_BN128 -DPERFORMANCE=Off -DWITH_PROCPS=Off
        -DUSE_PT_COMPRESSION=Off
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}>
        $<$<BOOL:${MSVC}>:"-DCMAKE_CXX_FLAGS=-GR -Gd -fp:precise -FS -EHa -MP">
        $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_DEBUG=-MTd>
        $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_RELEASE=-MT>
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
    LOG_BUILD 1
    INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target install
    BUILD_BYPRODUCTS "${libff_library}"
)
add_dependencies(libff mpir)

# Create snark imported library
add_library(libff::ff STATIC IMPORTED)
file(MAKE_DIRECTORY ${libff_inlcude_dir})
set_property(TARGET libff::ff PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET libff::ff PROPERTY IMPORTED_LOCATION_RELEASE ${libff_library})
set_property(TARGET libff::ff PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libff_inlcude_dir})
set_property(TARGET libff::ff PROPERTY INTERFACE_LINK_LIBRARIES MPIR::mpir)
add_dependencies(libff::ff libff)
