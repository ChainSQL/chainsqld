set(libcrypto "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}crypto${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(libssl "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ssl${CMAKE_STATIC_LIBRARY_SUFFIX}")

ExternalProject_Add(libgmssl
    PREFIX ${nih_cache_path}
    URL https://gitlab.peersafe.cn/chainsql_dependencies/gmssl/-/archive/feature/updateMerge4master/gmssl-feature-updateMerge4master.tar.gz
    URL_HASH SHA256=dbee50df5c407609ad7c62be6d30a5c6dee147cd4e95bded2d47bd62899d34ee
    CONFIGURE_COMMAND ./config -fPIC no-shared --prefix=${nih_cache_path} --openssldir=${nih_cache_path}/lib/ssl
    # CMAKE_ARGS
    #     $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    #     -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    #     -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    #     -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    #     $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}>
    #     $<$<BOOL:${MSVC}>:"-DCMAKE_CXX_FLAGS=-GR -Gd -fp:precise -FS -EHa -MP">
    #     $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_DEBUG=-MTd>
    #     $<$<BOOL:${MSVC}>:-DCMAKE_CXX_FLAGS_RELEASE=-MT>
    # BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
    # LOG_BUILD 1
    # INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target install
    INSTALL_COMMAND make install_sw
    BUILD_IN_SOURCE 1
    # BUILD_BYPRODUCTS "${libgmssl_library}"
)

set(OPENSSL_INCLUDE_DIRECTORIES ${nih_cache_path}/include/openssl)
set(OPENSSL_LINK_DIRECTORIES ${nih_cache_path}/lib)
if(openssl)
    unset(OPENSSL_ROOT_DIR)
else()
    set(OPENSSL_ROOT_DIR ${nih_cache_path})
endif()

# Create imported library
add_library(OpenSSL::SSL STATIC IMPORTED)
set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_LOCATION_RELEASE ${libssl})
set_target_properties(OpenSSL::SSL PROPERTIES INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2)
add_library(OpenSSL::Crypto STATIC IMPORTED)
set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION_RELEASE ${libcrypto})
set_property(TARGET OpenSSL::SSL PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${nih_cache_path}/include/openssl)
set_property(TARGET OpenSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${nih_cache_path}/include/openssl)
add_dependencies(OpenSSL::SSL ${libcrypto})
add_dependencies(OpenSSL::Crypto libgmssl)

# target_link_libraries (ripple_libs INTERFACE
#     OpenSSL::SSL
#     OpenSSL::Crypto)

exclude_if_included(libgmssl)
exclude_if_included(OpenSSL::SSL)
exclude_if_included(OpenSSL::Crypto)