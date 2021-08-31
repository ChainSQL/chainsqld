include(ExternalProject)

#set(prefix "${CMAKE_BINARY_DIR}/deps")
set(MPIR_LIBRARY "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}mpir${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(MPIR_INCLUDE_DIR "${nih_cache_path}/include")

ExternalProject_Add(mpir
    PREFIX ${nih_cache_path}
    GIT_REPOSITORY https://github.com/chfast/mpir.git
    GIT_TAG cmake
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=lib
        $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
        -DMPIR_GMP=On
        $<$<BOOL:${MSVC}>:"-DCMAKE_CXX_FLAGS=-GR -Gd -fp:precise -FS -EHa -MP">
        $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_DEBUG=-MTd>
        $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_RELEASE=-MT>
    BUILD_BYPRODUCTS "${MPIR_LIBRARY}"
)

add_library(MPIR::mpir STATIC IMPORTED)
set_property(TARGET MPIR::mpir PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET MPIR::mpir PROPERTY IMPORTED_LOCATION_RELEASE ${MPIR_LIBRARY})
set_property(TARGET MPIR::mpir PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MPIR_INCLUDE_DIR})
add_dependencies(MPIR::mpir mpir)
