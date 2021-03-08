set(libcrypto "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}crypto${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(libssl "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ssl${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(libgmssl_include_dir "${nih_cache_path}/include/openssl")

if(UNIX)
    set(GMSSL_CFG_CMD ./config -fPIC no-shared --prefix=${nih_cache_path} --openssldir=${nih_cache_path}/lib/ssl)
    set(GMSSL_INSTALL_CMD make install_sw)
    file(MAKE_DIRECTORY ${nih_cache_path}/lib)
    execute_process(COMMAND touch libcrypto.a WORKING_DIRECTORY ${nih_cache_path}/lib)
elseif(WIN32)
    set(libcrypto "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(libssl "${nih_cache_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libssl${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(GMSSL_CFG_CMD perl Configure VC-WIN64A no-shared no-asm --prefix=${nih_cache_path} --openssldir=${nih_cache_path}/lib/ssl)
    set(GMSSL_BUILD_CMD nmake)
    set(GMSSL_INSTALL_CMD nmake install_sw)
elseif(APPLE)
    set(GMSSL_CFG_CMD)
    set(GMSSL_INSTALL_CMD)
endif()

ExternalProject_Add(libgmssl
    PREFIX ${nih_cache_path}
    URL https://gitlab.peersafe.cn/chainsql_dependencies/gmssl/-/archive/feature/updateMerge4master/gmssl-feature-updateMerge4master.tar.gz
    URL_HASH SHA256=90b78724419b7fb1c5d3fee9f29a17f4c5d555a4fc25b26e287558bd99b36e13
    CONFIGURE_COMMAND ${GMSSL_CFG_CMD}
    BUILD_COMMAND ${GMSSL_BUILD_CMD}
    INSTALL_COMMAND ${GMSSL_INSTALL_CMD}
    BUILD_IN_SOURCE 1
    # BUILD_BYPRODUCTS "${libcrypto}"
)

set(OPENSSL_INCLUDE_DIRECTORIES ${nih_cache_path}/include/openssl)
set(OPENSSL_LINK_DIRECTORIES ${nih_cache_path}/lib)
if(openssl)
    message(STATUS "Unset gmssl path")
    unset(OPENSSL_ROOT_DIR)
else()
    message(STATUS "Set gmssl path")
    set(OPENSSL_ROOT_DIR ${nih_cache_path})
endif()

file(MAKE_DIRECTORY ${libgmssl_include_dir})
# Create imported library
add_library(OpenSSL::SSL STATIC IMPORTED)
set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_LOCATION_RELEASE ${libssl})
set_target_properties(OpenSSL::SSL PROPERTIES INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2)
add_library(OpenSSL::Crypto STATIC IMPORTED)
set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION_RELEASE ${libcrypto})
set_property(TARGET OpenSSL::SSL PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libgmssl_include_dir})
set_property(TARGET OpenSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libgmssl_include_dir})
add_dependencies(OpenSSL::SSL ${libcrypto})
add_dependencies(OpenSSL::Crypto libgmssl)

find_package (ZLIB)
set (has_zlib FALSE)
if (TARGET ZLIB::ZLIB)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
  set (has_zlib TRUE)
endif ()

# target_link_libraries (ripple_libs INTERFACE
#     OpenSSL::SSL
#     OpenSSL::Crypto)

exclude_if_included(libgmssl)
exclude_if_included(OpenSSL::SSL)
exclude_if_included(OpenSSL::Crypto)