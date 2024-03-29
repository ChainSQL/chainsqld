cmake_minimum_required (VERSION 3.10.0)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif ()

project (chainsqld)

set(enableHDGm FALSE CACHE BOOL "Enable hard GM algorithm")
set(enableTest FALSE CACHE BOOL "Enable Unit Test Module")
# if don't use -Dopenssl=TRUE, default use both china gm and international crypto alg
set(openssl FALSE CACHE BOOL "use openssl, only support international crypto alg")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Builds/CMake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Builds/CMake/deps")

include (CheckCXXCompilerFlag)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.11)
  include (FetchContent)
endif ()
if (MSVC AND CMAKE_VERSION VERSION_LESS 3.12)
  message (FATAL_ERROR "MSVC requires cmake 3.12 or greater for proper boost support")
endif ()
include (ExternalProject)
include (CMakeFuncs) # must come *after* ExternalProject b/c it overrides one function in EP
include (ProcessorCount)
if (target)
  message (FATAL_ERROR "The target option has been removed - use native cmake options to control build")
endif ()

include(RippledSanity)
include(ChainsqldVersion)
include(RippledSettings)
include(RippledNIH)
#include(RippledRelease)
# this check has to remain in the top-level cmake
# because of the early return statement
if (packages_only)
  if (NOT TARGET rpm)
    message (FATAL_ERROR "packages_only requested, but targets were not created - is docker installed?")
  endif()
  return ()
endif ()

include(RippledCompiler)
include(RippledInterface)

###

if(openssl)
    message(STATUS "Use OpenSSL")
    include(deps/OpenSSL)
else()
    message(STATUS "Use GmSSL")
    include(deps/LibGMSSL)
endif()
# include(deps/LibGMSSL)
include(deps/Boost)
include(deps/Secp256k1)
include(deps/Ed25519-donna)
include(deps/Lz4)
include(deps/Libarchive)
include(deps/Sqlite)
include(deps/Soci)
include(deps/Mysql)
include(deps/Snappy)
include(deps/Rocksdb)
include(deps/Nudb)
include(deps/date)
include(deps/Protobuf)
include(deps/gRPC)
include(deps/LibFF)
include(deps/Prometheus-cpp)
###

add_subdirectory(src/eth/evmc)
add_subdirectory(src/intx)
add_subdirectory(src/wasmvm)

include(ChainsqldCore)
include(RippledInstall)
include(RippledCov)
include(RippledMultiConfig)
#include(RippledDocs)
#include(RippledValidatorKeys)

