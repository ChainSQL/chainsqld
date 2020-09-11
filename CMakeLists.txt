# !!! The official build system is SConstruct !!!
# This is an experimental cmake build file for chainsqld
#
# cmake support in chainsqld. Currently supports:
#
#  * unity/nounity debug/release
#  * running protobuf
#  * sanitizer builds
#  * optional release build with assert turned on
#  * `target` variable to easily set compiler/debug/unity
#     (i.e. -Dtarget=gcc.debug.nounity)
#  * gcc/clang/visual studio/xcode
#  * linux/mac/win
#  * gcc 4 ABI, when needed
#  * ninja builds
#  * check openssl version on linux
#  * static builds (swd TBD: needs to be tested by building & deploying on different systems)
#  * jemalloc enabled builds (linux and macos only)
#  * perf builds (linux only) - which just sets recommended compiler flags
#          for running perf on the executable
#
# Notes:
#  * Use the -G"Visual Studio 14 2015 Win64" generator, or the "VS2015 x86 x64
#    Cross Tools" Command Prompt on Windows. Without this a 32-bit project will be
#    created. There is no way to set the generator or force a 64-bit build in
#    CMakeLists.txt (setting CMAKE_GENERATOR_PLATFORM won't work). The best solution
#    may be to wrap cmake with a script.
#
#  * Ninja command line builds seem to work under Windows, but only from within
#    the "VS2015 x86 x64 Cross Tools" Command Prompt.
#
#  * It is not possible to generate a visual studio project on linux or
#    mac. The visual studio generator is only available on windows.
#
#  * The Visual Studio solution will be generated with two projects, one
#    unity, one non-unity. Which is default depends on the nounity flag in
#    -Dtarget. Unity targets will create `chainsqld` and `chainsqld_classic`.
#    Non-unity targets will create `chainsqld` and `chainsqld_unity`. In either
#    case, only the `chainsqld` build will be enabled by default. It does
#    not appear possible to include both unity and non-unity configs in one
#    project and disable compilation based on configuration.
#
#  * Language is _much_ worse than python, poor documentation and "quirky"
#    language support (for example, generator expressions can only be used
#    in limited contexts and seem to work differently based on
#    context (set_property can set multiple values, add_compile_options
#    can not/or is buggy)
#
#  * Could not call out to `sed` because cmake messed with the regular
#    expression before calling the external command. I did not see a way
#    around this.
#
#  * Makefile generators want to be single target. It wants a separate
#    directory for each target type. I saw some mentions on the web for
#    ways around this bug haven't look into it. The visual studio project
#    does support debug/release configurations in the same project (but
#    not unity/non-unity).

############################################################

#########################################################
# CMAKE_C_COMPILER and CMAKE_CXX_COMPILER must be defined
# before the project statement; However, the project
# statement will clear CMAKE_BUILD_TYPE. CACHE variables,
# along with the order of this code, are used to work
# around these constraints.
#
# Don't put any code above or in this block, unless it
# has similar constraints.
cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Builds/CMake")
include(CMakeFuncs)
set(openssl_min 1.0.2)
parse_target()
project(chainsqld)
#########################################################

set(enableTest FALSE CACHE BOOL "Enable Unit Test Module")
#########################################################

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  set(dir "build")
  set(cmd "cmake")
  if (target)
    set(dir "${dir}/${target}")
    set(cmd "${cmd} -Dtarget=${target}")
  elseif(CMAKE_BUILD_TYPE)
    set(dir "${dir}/${CMAKE_BUILD_TYPE}")
    set(cmd "${cmd} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
  else()
    set(dir "${dir}/default")
  endif()
  set(cmd "${cmd} ${CMAKE_SOURCE_DIR}")

  message(FATAL_ERROR "Builds are not allowed in ${CMAKE_SOURCE_DIR}.\n"
    "Instead:\n"
    "1) Remove the CMakeCache.txt file and CMakeFiles directory "
   "from ${CMAKE_SOURCE_DIR}.\n"
    "2) Create a directory to hold your build files, for example: ${dir}.\n"
    "3) Change to that directory.\n"
    "4) Run cmake targetting ${CMAKE_SOURCE_DIR}, for example: ${cmd}")
endif()
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
	if(NOT "${CMAKE_GENERATOR}" MATCHES "2019" AND
		NOT ("${CMAKE_GENERATOR}" MATCHES .*Win64.*))
	message(FATAL_ERROR "Visual Studio 32-bit build is unsupported. Use
		-G\"${CMAKE_GENERATOR} Win64\"")
	endif()
endif()

setup_build_cache()

if(nonunity)
    get_cmake_property(allvars VARIABLES)
    string(REGEX MATCHALL "[^;]*(DEBUG|RELEASE)[^;]*" matchvars "${allvars}")
    foreach(var IN LISTS matchvars)
        string(REGEX REPLACE "(DEBUG|RELEASE)" "\\1CLASSIC" newvar ${var})
        set(${newvar} ${${var}})
    endforeach()

    get_cmake_property(allvars CACHE_VARIABLES)
    string(REGEX MATCHALL "[^;]*(DEBUG|RELEASE)[^;]*" matchvars "${allvars}")
    foreach(var IN LISTS matchvars)
        string(REGEX REPLACE "(DEBUG|RELEASE)" "\\1CLASSIC" newvar ${var})
        set(${newvar} ${${var}} CACHE STRING "Copied from ${var}")
    endforeach()
endif()

determine_build_type()

check_gcc4_abi()

############################################################

include_directories(
  src
  src/beast
  src/beast/include
  src/beast/extras
  src/nudb/include
  src/soci/src
  src/soci/include
  src/eth/evmc/include
  src/intx/include)

special_build_flags()

# setting output directory
set(bin_output_directory "${CMAKE_SOURCE_DIR}/build")
if (target)
    set(bin_output_directory "${bin_output_directory}/${target}")
elseif(CMAKE_BUILD_TYPE)
    set(bin_output_directory "${bin_output_directory}/${CMAKE_BUILD_TYPE}")
else()
    set(bin_output_directory "${bin_output_directory}/default")
endif()
set(EXECUTABLE_OUTPUT_PATH ${bin_output_directory})
set(LIBRARY_OUTPUT_PATH ${bin_output_directory})

############################################################

use_boost(
  # resist the temptation to alphabetize these. coroutine
  # must come before context.
  chrono
  coroutine
  context
  date_time
  filesystem
  program_options
  regex
  serialization
  system
  thread)

use_pthread()

#use_tbb()

use_openssl(${openssl_min})

use_protobuf()

use_mysql()

setup_build_boilerplate()

############################################################

add_library (chainsql_libs INTERFACE)

#[===================================================================[
   NIH prefix path..this is where we will download
   and build any ExternalProjects, and they will hopefully
   survive across build directory deletion (manual cleans)
#]===================================================================]
get_property (is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (is_multiconfig STREQUAL "NOTFOUND")
  if (${CMAKE_GENERATOR} STREQUAL "Xcode" OR ${CMAKE_GENERATOR} MATCHES "^Visual Studio")
    set (is_multiconfig TRUE)
  endif ()
endif ()

string (REGEX REPLACE "[ \\/%]+" "_" gen_for_path ${CMAKE_GENERATOR})
string (TOLOWER ${gen_for_path} gen_for_path)
# HACK: trying to shorten paths for windows CI (which hits 260 MAXPATH easily)
# @see:  https://issues.jenkins-ci.org/browse/JENKINS-38706?focusedCommentId=339847
string (REPLACE "visual_studio" "vs" gen_for_path ${gen_for_path})
if (NOT DEFINED NIH_CACHE_ROOT)
  if (DEFINED ENV{NIH_CACHE_ROOT})
    set (NIH_CACHE_ROOT $ENV{NIH_CACHE_ROOT})
  else ()
    set (NIH_CACHE_ROOT "${CMAKE_SOURCE_DIR}/.nih_c")
  endif ()
endif ()
set (nih_cache_path
  "${NIH_CACHE_ROOT}/${gen_for_path}/${CMAKE_CXX_COMPILER_ID}_${CMAKE_CXX_COMPILER_VERSION}")
if (NOT is_multiconfig)
  set (nih_cache_path "${nih_cache_path}/${CMAKE_BUILD_TYPE}")
endif ()
file(TO_CMAKE_PATH "${nih_cache_path}" nih_cache_path)
message (STATUS "NIH-EP cache path: ${nih_cache_path}")
## two convenience variables:
set (ep_lib_prefix ${CMAKE_STATIC_LIBRARY_PREFIX})
set (ep_lib_suffix ${CMAKE_STATIC_LIBRARY_SUFFIX})

# this is a setting for FetchContent and needs to be
# a cache variable
# https://cmake.org/cmake/help/latest/module/FetchContent.html#populating-the-content
set (FETCHCONTENT_BASE_DIR ${nih_cache_path} CACHE STRING "" FORCE)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

#[===================================================================[
   dep: lz4
#]===================================================================]

ExternalProject_Add (lz4
  PREFIX ${CMAKE_SOURCE_DIR}/deps
  DOWNLOAD_NO_PROGRESS 1
  DOWNLOAD_NAME lz4-1.8.2.tar.gz
  URL https://github.com/lz4/lz4/archive/v1.8.2.tar.gz
  URL_HASH MD5=c2082b751d75550ec4dc163c773e1a71
  #GIT_REPOSITORY https://github.com/lz4/lz4.git
  #GIT_TAG v1.8.2
  SOURCE_SUBDIR contrib/cmake_unofficial
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
    -DCMAKE_DEBUG_POSTFIX=_d
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBUILD_STATIC_LIBS=ON
    -DBUILD_SHARED_LIBS=OFF
    $<$<BOOL:${MSVC}>:
      "-DCMAKE_C_FLAGS=-GR -Gd -fp:precise -FS -MP"
      "-DCMAKE_C_FLAGS_DEBUG=-MTd"
      "-DCMAKE_C_FLAGS_RELEASE=-MT"
    >
  LOG_BUILD ON
  LOG_CONFIGURE ON
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config $<CONFIG>
    --target lz4_static
    $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:--parallel ${ep_procs}>
    $<$<BOOL:${is_multiconfig}>:
      COMMAND
        ${CMAKE_COMMAND} -E copy
        <BINARY_DIR>/$<CONFIG>/${ep_lib_prefix}lz4$<$<CONFIG:Debug>:_d>${ep_lib_suffix}
        <BINARY_DIR>
      >
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
    <BINARY_DIR>/${ep_lib_prefix}lz4${ep_lib_suffix}
    <BINARY_DIR>/${ep_lib_prefix}lz4_d${ep_lib_suffix}
)
ExternalProject_Get_Property (lz4 BINARY_DIR)
ExternalProject_Get_Property (lz4 SOURCE_DIR)
if (CMAKE_VERBOSE_MAKEFILE)
  print_ep_logs (lz4)
endif ()
#message("xxx ${SOURCE_DIR}")
add_library (lz4_lib STATIC IMPORTED GLOBAL)
file (MAKE_DIRECTORY ${SOURCE_DIR}/lz4)
set_target_properties (lz4_lib PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/${ep_lib_prefix}lz4_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/${ep_lib_prefix}lz4${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    ${SOURCE_DIR}/lib)
add_dependencies (lz4_lib lz4)
target_link_libraries (chainsql_libs INTERFACE lz4_lib)
exclude_if_included (lz4)
exclude_if_included (lz4_lib)


#[===================================================================[
   dep: libarchive
#]===================================================================]
set (lib_post "")
if (MSVC)
  set (lib_post "_static")
endif ()
ExternalProject_Add (libarchive
  PREFIX ${CMAKE_SOURCE_DIR}/deps
  DOWNLOAD_NO_PROGRESS 1
  DOWNLOAD_NAME libarchive-3.3.3.tar.gz
  URL https://github.com/libarchive/libarchive/archive/v3.3.3.tar.gz
  URL_HASH MD5=c591a2781609e7d0c47049a0f061ce99
  #PREFIX ${nih_cache_path}
  #GIT_REPOSITORY https://github.com/libarchive/libarchive.git
  #GIT_TAG v3.3.3
  CMAKE_ARGS
    # passing the compiler seems to be needed for windows CI, sadly
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
    -DCMAKE_DEBUG_POSTFIX=_d
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DENABLE_LZ4=ON
    -ULZ4_*
    -DLZ4_INCLUDE_DIR=$<JOIN:$<TARGET_PROPERTY:lz4_lib,INTERFACE_INCLUDE_DIRECTORIES>,::>
    # because we are building a static lib, this lz4 library doesn't
    # actually matter since you can't generally link static libs to other static
    # libs. The include files are needed, but the library itself is not (until
    # we link our application, at which point we use the lz4 we built above).
    # nonetheless, we need to provide a library to libarchive else it will
    # NOT include lz4 support when configuring
    -DLZ4_LIBRARY=$<IF:$<CONFIG:Debug>,$<TARGET_PROPERTY:lz4_lib,IMPORTED_LOCATION_DEBUG>,$<TARGET_PROPERTY:lz4_lib,IMPORTED_LOCATION_RELEASE>>
    -DENABLE_WERROR=OFF
    -DENABLE_TAR=OFF
    -DENABLE_TAR_SHARED=OFF
    -DENABLE_INSTALL=ON
    -DENABLE_NETTLE=OFF
    -DENABLE_OPENSSL=OFF
    -DENABLE_LZO=OFF
    -DENABLE_LZMA=OFF
    -DENABLE_ZLIB=OFF
    -DENABLE_BZip2=OFF
    -DENABLE_LIBXML2=OFF
    -DENABLE_EXPAT=OFF
    -DENABLE_PCREPOSIX=OFF
    -DENABLE_LibGCC=OFF
    -DENABLE_CNG=OFF
    -DENABLE_CPIO=OFF
    -DENABLE_CPIO_SHARED=OFF
    -DENABLE_CAT=OFF
    -DENABLE_CAT_SHARED=OFF
    -DENABLE_XATTR=OFF
    -DENABLE_ACL=OFF
    -DENABLE_ICONV=OFF
    -DENABLE_TEST=OFF
    -DENABLE_COVERAGE=OFF
    $<$<BOOL:${MSVC}>:
      "-DCMAKE_C_FLAGS=-GR -Gd -fp:precise -FS -MP"
      "-DCMAKE_C_FLAGS_DEBUG=-MTd"
      "-DCMAKE_C_FLAGS_RELEASE=-MT"
    >
  LIST_SEPARATOR ::
  LOG_BUILD ON
  LOG_CONFIGURE ON
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config $<CONFIG>
    --target archive_static
    $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:--parallel ${ep_procs}>
    $<$<BOOL:${is_multiconfig}>:
      COMMAND
        ${CMAKE_COMMAND} -E copy
          <BINARY_DIR>/libarchive/$<CONFIG>/${ep_lib_prefix}archive${lib_post}$<$<CONFIG:Debug>:_d>${ep_lib_suffix}
          <BINARY_DIR>/libarchive
      >
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  DEPENDS lz4
  BUILD_BYPRODUCTS
    <BINARY_DIR>/libarchive/${ep_lib_prefix}archive${lib_post}${ep_lib_suffix}
    <BINARY_DIR>/libarchive/${ep_lib_prefix}archive${lib_post}_d${ep_lib_suffix}
)
ExternalProject_Get_Property (libarchive BINARY_DIR)
ExternalProject_Get_Property (libarchive SOURCE_DIR)
if (CMAKE_VERBOSE_MAKEFILE)
  print_ep_logs (libarchive)
endif ()
add_library (archive_lib STATIC IMPORTED GLOBAL)
file (MAKE_DIRECTORY ${SOURCE_DIR}/libarchive)
set_target_properties (archive_lib PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/libarchive/${ep_lib_prefix}archive${lib_post}_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/libarchive/${ep_lib_prefix}archive${lib_post}${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    ${SOURCE_DIR}/libarchive
  INTERFACE_COMPILE_DEFINITIONS
    LIBARCHIVE_STATIC)
add_dependencies (archive_lib libarchive)
target_link_libraries (archive_lib INTERFACE lz4_lib)
target_link_libraries (chainsql_libs INTERFACE archive_lib)
exclude_if_included (libarchive)
exclude_if_included (archive_lib)

#[===================================================================[
   dep: snappy
#]===================================================================]

ExternalProject_Add (snappy
  PREFIX ${CMAKE_SOURCE_DIR}/deps
  #DOWNLOAD_NO_PROGRESS 1
  #DOWNLOAD_NAME snappy-1.1.7.tar.gz
  #URL https://github.com/google/snappy/archive/v1.1.7.tar.gz
  #URL_HASH MD5=c591a2781609e7d0c47049a0f061ce99
  GIT_REPOSITORY https://github.com/google/snappy.git
  GIT_TAG 1.1.7
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
    -DCMAKE_DEBUG_POSTFIX=_d
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DSNAPPY_BUILD_TESTS=OFF
    $<$<BOOL:${MSVC}>:
      "-DCMAKE_CXX_FLAGS=-GR -Gd -fp:precise -FS -MP"
      "-DCMAKE_CXX_FLAGS_DEBUG=-MTd"
      "-DCMAKE_CXX_FLAGS_RELEASE=-MT"
    >
  LOG_BUILD ON
  LOG_CONFIGURE ON
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config $<CONFIG>
    $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:--parallel ${ep_procs}>
    $<$<BOOL:${is_multiconfig}>:
      COMMAND
        ${CMAKE_COMMAND} -E copy
        <BINARY_DIR>/$<CONFIG>/${ep_lib_prefix}snappy$<$<CONFIG:Debug>:_d>${ep_lib_suffix}
        <BINARY_DIR>
      >
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
    <BINARY_DIR>/${ep_lib_prefix}snappy${ep_lib_suffix}
    <BINARY_DIR>/${ep_lib_prefix}snappy_d${ep_lib_suffix}
)
ExternalProject_Get_Property (snappy BINARY_DIR)
ExternalProject_Get_Property (snappy SOURCE_DIR)
if (CMAKE_VERBOSE_MAKEFILE)
  print_ep_logs (snappy)
endif ()
add_library (snappy_lib STATIC IMPORTED GLOBAL)
file (MAKE_DIRECTORY ${SOURCE_DIR}/snappy)
set_target_properties (snappy_lib PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/${ep_lib_prefix}snappy_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/${ep_lib_prefix}snappy${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    "${SOURCE_DIR};${BINARY_DIR}")
add_dependencies (snappy_lib snappy)
target_link_libraries (chainsql_libs INTERFACE snappy_lib)
exclude_if_included (snappy)
exclude_if_included (snappy_lib)

#[===================================================================[
   dep: rocksdb
#]===================================================================]

ExternalProject_Add (rocksdb
  PREFIX ${CMAKE_SOURCE_DIR}/deps
  URL https://github.com/facebook/rocksdb/archive/v5.17.2.tar.gz
  URL_HASH MD5=6d5ea558501c4b7f474fd59d4fbcfa3a
  DOWNLOAD_NAME rocksdb-5.17.2.tar.gz
  PATCH_COMMAND
    # only used by windows build
    ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/Builds/CMake/rocks_thirdparty.inc
    <SOURCE_DIR>/thirdparty.inc
  COMMAND
    # fixup their build version file to keep the values
    # from changing always
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/Builds/CMake/rocksdb_build_version.cc.in
    <SOURCE_DIR>/util/build_version.cc.in
  CMAKE_ARGS
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
    -DCMAKE_DEBUG_POSTFIX=_d
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DWITH_JEMALLOC=$<IF:$<BOOL:${jemalloc}>,ON,OFF>
    -DWITH_SNAPPY=ON
    -DWITH_LZ4=ON
    -DWITH_ZLIB=OFF
    -DUSE_RTTI=ON
    -DWITH_ZSTD=OFF
    -DWITH_GFLAGS=OFF
    -DWITH_BZ2=OFF
    -ULZ4_*
    -DLZ4_INCLUDE_DIR=$<JOIN:$<TARGET_PROPERTY:lz4_lib,INTERFACE_INCLUDE_DIRECTORIES>,::>
    -DLZ4_LIBRARIES=$<IF:$<CONFIG:Debug>,$<TARGET_PROPERTY:lz4_lib,IMPORTED_LOCATION_DEBUG>,$<TARGET_PROPERTY:lz4_lib,IMPORTED_LOCATION_RELEASE>>
    -DLZ4_FOUND=ON
    -USNAPPY_*
    -DSNAPPY_INCLUDE_DIR=$<JOIN:$<TARGET_PROPERTY:snappy_lib,INTERFACE_INCLUDE_DIRECTORIES>,::>
    -DSNAPPY_LIBRARIES=$<IF:$<CONFIG:Debug>,$<TARGET_PROPERTY:snappy_lib,IMPORTED_LOCATION_DEBUG>,$<TARGET_PROPERTY:snappy_lib,IMPORTED_LOCATION_RELEASE>>
    -DSNAPPY_FOUND=ON
    -DWITH_MD_LIBRARY=OFF
    -DWITH_RUNTIME_DEBUG=$<IF:$<CONFIG:Debug>,ON,OFF>
    -DFAIL_ON_WARNINGS=OFF
    -DWITH_ASAN=OFF
    -DWITH_TSAN=OFF
    -DWITH_UBSAN=OFF
    -DWITH_NUMA=OFF
    -DWITH_TBB=OFF
    -DWITH_WINDOWS_UTF8_FILENAMES=OFF
    -DWITH_XPRESS=OFF
    -DPORTABLE=ON
    -DFORCE_SSE42=OFF
    -DDISABLE_STALL_NOTIF=OFF
    -DOPTDBG=ON
    -DROCKSDB_LITE=OFF
    -DWITH_FALLOCATE=ON
    -DWITH_LIBRADOS=OFF
    -DWITH_JNI=OFF
    -DROCKSDB_INSTALL_ON_WINDOWS=OFF
    -DWITH_TESTS=OFF
    -DWITH_TOOLS=OFF
    $<$<BOOL:${MSVC}>:
      "-DCMAKE_CXX_FLAGS=-GR -Gd -fp:precise -FS -MP /DNDEBUG"
    >
    $<$<NOT:$<BOOL:${MSVC}>>:
      "-DCMAKE_CXX_FLAGS=-DNDEBUG"
    >
  LOG_BUILD ON
  LOG_CONFIGURE ON
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config $<CONFIG>
    $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:--parallel ${ep_procs}>
    $<$<BOOL:${is_multiconfig}>:
      COMMAND
        ${CMAKE_COMMAND} -E copy
        <BINARY_DIR>/$<CONFIG>/${ep_lib_prefix}rocksdb$<$<CONFIG:Debug>:_d>${ep_lib_suffix}
        <BINARY_DIR>
      >
  LIST_SEPARATOR ::
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  DEPENDS snappy lz4
  BUILD_BYPRODUCTS
    <BINARY_DIR>/${ep_lib_prefix}rocksdb${ep_lib_suffix}
    <BINARY_DIR>/${ep_lib_prefix}rocksdb_d${ep_lib_suffix}
)
ExternalProject_Get_Property (rocksdb BINARY_DIR)
ExternalProject_Get_Property (rocksdb SOURCE_DIR)
if (CMAKE_VERBOSE_MAKEFILE)
  print_ep_logs (rocksdb)
endif ()
add_library (rocksdb_lib STATIC IMPORTED GLOBAL)
file (MAKE_DIRECTORY ${SOURCE_DIR}/include)
set_target_properties (rocksdb_lib PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/${ep_lib_prefix}rocksdb_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/${ep_lib_prefix}rocksdb${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    ${SOURCE_DIR}/include
  INTERFACE_COMPILE_DEFINITIONS
    RIPPLE_ROCKSDB_AVAILABLE=1)
add_dependencies (rocksdb_lib rocksdb)
target_link_libraries (rocksdb_lib INTERFACE snappy_lib lz4_lib)
if (MSVC)
  target_link_libraries (rocksdb_lib INTERFACE rpcrt4)
endif ()
target_link_libraries (chainsql_libs INTERFACE rocksdb_lib)
exclude_if_included (rocksdb)
exclude_if_included (rocksdb_lib)

set(soci_extra_includes
  -I"${CMAKE_SOURCE_DIR}"/src/soci/src/core
  -I"${CMAKE_SOURCE_DIR}"/src/soci/include/private
  -I"${CMAKE_SOURCE_DIR}"/src/sqlite)

# Unity sources
prepend(beast_unity_srcs
src/ripple/beast/unity/
beast_insight_unity.cpp
beast_net_unity.cpp
beast_utility_unity.cpp)

prepend(ripple_unity_srcs
src/ripple/unity/
app_consensus.cpp
app_ledger.cpp
app_ledger_impl.cpp
app_main1.cpp
app_main2.cpp
app_misc.cpp
app_misc_impl.cpp
app_paths.cpp
app_tx.cpp
conditions.cpp
consensus.cpp
core.cpp
basics.cpp
crypto.cpp
ledger.cpp
net.cpp
nodestore.cpp
overlay1.cpp
overlay2.cpp
peerfinder.cpp
json.cpp
protocol.cpp
rpcx1.cpp
rpcx2.cpp
shamap.cpp
server.cpp)

if(enableTest)
    prepend(test_unity_srcs
    src/test/unity/
    app_test_unity1.cpp
    app_test_unity2.cpp
    basics_test_unity.cpp
    beast_test_unity1.cpp
    beast_test_unity2.cpp
    conditions_test_unity.cpp
    consensus_test_unity.cpp
    core_test_unity.cpp
    json_test_unity.cpp
    ledger_test_unity.cpp
    nodestore_test_unity.cpp
    overlay_test_unity.cpp
    peerfinder_test_unity.cpp
    protocol_test_unity.cpp
    resource_test_unity.cpp
    rpc_test_unity.cpp
    server_test_unity.cpp
    server_status_test_unity.cpp
    shamap_test_unity.cpp
    jtx_unity1.cpp
    jtx_unity2.cpp
    csf_unity.cpp
    vm_test_unity.cpp)
else()
    #set(test_unity_srcs)
	prepend(test_unity_srcs
    src/test/unit_test/
	multi_runner.cpp
	)

	add_with_props(test_unity_srcs src/test/jtx/impl/envconfig.cpp -I"${CMAKE_SOURCE_DIR}"/src/test/jtx)
endif()

prepend(peersafe_unity_srcs
src/peersafe/unity/
gm_encrypt.cpp
app_table.cpp
app_contract.cpp
app_sql.cpp)

prepend(vm_unity_srcs
src/eth/unity/
vm_unity.cpp)



list(APPEND chainsqld_src_unity ${beast_unity_srcs} ${ripple_unity_srcs} ${peersafe_unity_srcs} ${vm_unity_srcs} ${test_unity_srcs})

add_with_props(chainsqld_src_unity src/ripple/unity/soci_ripple.cpp ${soci_extra_includes})

############################################################

# Non-unity sources
file(GLOB_RECURSE core_srcs src/ripple/core/*.cpp)
add_with_props(chainsqld_src_nonunity "${core_srcs}"
    -I"${CMAKE_SOURCE_DIR}"/src/soci/src/core
    -I"${CMAKE_SOURCE_DIR}"/src/sqlite)

set(non_unity_srcs ${core_srcs})

add_subdirectory(src/eth/evmc)
add_subdirectory(src/intx)

foreach(curdir
        beast/clock
        beast/container
        beast/insight
        beast/net
        beast/utility
        app
        basics
        conditions
        crypto
        consensus
        json
        ledger
        legacy
        net
        nodestore
        overlay
        peerfinder
        protocol
        rpc
        server
        shamap)
    file(GLOB_RECURSE cursrcs src/ripple/${curdir}/*.cpp)
    list(APPEND chainsqld_src_nonunity "${cursrcs}")
    list(APPEND non_unity_srcs "${cursrcs}")
endforeach()

foreach(curdir
        app
        basics
        crypto
        gmencrypt
        protocol
        rpc)
    file(GLOB_RECURSE cursrcs src/peersafe/${curdir}/*.cpp)
    list(APPEND chainsqld_src_nonunity "${cursrcs}")
    list(APPEND non_unity_srcs "${cursrcs}")
endforeach()

foreach(curdir
        vm)
    file(GLOB_RECURSE cursrcs src/eth/${curdir}/*.cpp)
    list(APPEND chainsqld_src_nonunity "${cursrcs}")
    list(APPEND non_unity_srcs "${cursrcs}")
endforeach()

# unit test sources
if(enableTest)
    foreach(curdir
            app
            basics
            beast
            conditions
            consensus
            core
            csf
            json
            jtx
            ledger
            nodestore
            overlay
            peerfinder
            protocol
            resource
            rpc
            server
            shamap
            unit_test
            vm)
        file(GLOB_RECURSE cursrcs src/test/${curdir}/*.cpp)
        list(APPEND test_srcs "${cursrcs}")
    endforeach()

    add_with_props(chainsqld_src_nonunity "${test_srcs}")

    list(APPEND non_unity_srcs "${test_srcs}")
else()
	prepend(test_srcs
    src/test/unit_test/
	multi_runner.cpp
	)

	add_with_props(test_srcs src/test/jtx/impl/envconfig.cpp -I"${CMAKE_SOURCE_DIR}"/src/test/jtx)
	
	add_with_props(chainsqld_src_nonunity "${test_srcs}")
    list(APPEND non_unity_srcs "${test_srcs}")
endif()

if(WIN32 OR is_xcode)
    # chainsqld headers. Only needed for IDEs.
    file(GLOB_RECURSE chainsqld_headers src/*.h src/*.hpp)
    list(APPEND chainsqld_headers Builds/CMake/CMakeFuncs.cmake)
    foreach(curdir
            beast/asio
            beast/core
            beast/crypto
            beast/cxx17
            beast/hash
            proto
            resource
            validators
            websocket)
        file(GLOB_RECURSE cursrcs src/ripple/${curdir}/*.cpp)
        list(APPEND chainsqld_headers "${cursrcs}")
    endforeach()
    list(APPEND chainsqld_src_nonunity "${chainsqld_headers}")

    set_property(
        SOURCE ${chainsqld_headers}
        APPEND
        PROPERTY HEADER_FILE_ONLY
        true)
        # Doesn't work
        # $<OR:$<CONFIG:Debug>,$<CONFIG:Release>>)
endif()
############################################################

add_with_props(chainsqld_src_all src/ripple/unity/soci.cpp
  ${soci_extra_includes})

if (NOT is_msvc)
  set(no_unused_w -Wno-unused-function)
else()
  unset(no_unused_w)
endif()

add_with_props(chainsqld_src_all src/ripple/unity/secp256k1.cpp
  -I"${CMAKE_SOURCE_DIR}"/src/secp256k1
  ${no_unused_w}
  )

foreach(cursrc
    src/ripple/beast/unity/beast_hash_unity.cpp
    src/ripple/unity/beast.cpp
    src/ripple/unity/protobuf.cpp
    src/ripple/unity/ripple.proto.cpp
    src/ripple/unity/resource.cpp)

  add_with_props(chainsqld_src_all ${cursrc}
    ${rocks_db_system_header}
    )

endforeach()

if (NOT is_msvc)
  set(extra_props -Wno-array-bounds)
else()
  unset(extra_props)
endif()

add_with_props(chainsqld_src_all src/sqlite/sqlite_unity.c
  ${extra_props})

add_with_props(chainsqld_src_all src/ripple/unity/ed25519_donna.c
  -I"${CMAKE_SOURCE_DIR}"/src/ed25519-donna)
  
if (is_gcc)
  set(no_init_w -Wno-maybe-uninitialized)
else()
  unset(no_init_w)
endif()

if (NOT is_msvc)
  set(no_unused_w -Wno-unused-function)
endif()


if (APPLE AND is_clang)
  list(APPEND chainsqld_src_all src/ripple/unity/beastobjc.mm)
endif()

list(APPEND chainsqld_src_unity "${chainsqld_src_all}")
list(APPEND chainsqld_src_nonunity "${chainsqld_src_all}")

############################################################

if (WIN32 OR is_xcode)
  group_sources(src)
  group_sources(docs)
  group_sources(Builds)
endif()

if(unity)
    add_executable(chainsqld ${chainsqld_src_unity} ${PROTO_HDRS})
    add_executable(chainsqld_classic EXCLUDE_FROM_ALL ${chainsqld_src_nonunity} ${PROTO_HDRS})
    set(other_target chainsqld_classic)
else()
    add_executable(chainsqld ${chainsqld_src_nonunity} ${PROTO_HDRS})
    add_executable(chainsqld_unity EXCLUDE_FROM_ALL ${chainsqld_src_unity} ${PROTO_HDRS})
    set(other_target chainsqld_unity)
endif()
list(APPEND targets "chainsqld")
list(APPEND targets ${other_target})
# Not the same as EXCLUDE_FROM_ALL. Prevents Visual Studio from building the
# other_target when the user builds the solution (default when pressing <F7>)
set_property(TARGET ${other_target} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD true)

find_program(
  B2_EXE
  NAMES b2
  HINTS ${BOOST_ROOT}
  PATHS ${BOOST_ROOT}
  DOC "Location of the b2 build executable from Boost")
if(${B2_EXE} STREQUAL "B2_EXE-NOTFOUND")
  message(WARNING
    "Boost b2 executable not found. docs target will not be buildable")
elseif(NOT BOOST_ROOT)
    if(Boost_INCLUDE_DIRS)
        set(BOOST_ROOT ${Boost_INCLUDE_DIRS})
    else()
        get_filename_component(BOOST_ROOT ${B2_EXE} DIRECTORY)
    endif()
endif()
# The value for BOOST_ROOT will be determined based on
#   1) The environment BOOST_ROOT
#   2) The Boost_INCLUDE_DIRS found by `get_boost`
#   3) The folder the `b2` executable is found in.
# If those checks don't yield the correct path, BOOST_ROOT
# can be defined on the cmake command line:
#    cmake <path> -DBOOST_ROOT=<boost_path>
if(BOOST_ROOT)
    set(B2_PARAMS "-sBOOST_ROOT=${BOOST_ROOT}")
endif()

# Find bash to help Windows avoid file association problems
find_program(
    BASH_EXE
    NAMES bash sh
    DOC "Location of the bash shell executable"
    )
if(${BASH_EXE} STREQUAL "BASH_EXE-NOTFOUND")
    message(WARNING
        "Unable to find bash executable. docs target may not be buildable")
    set(BASH_EXE "")
endif()

add_custom_target(docs
  COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH} " ${BASH_EXE} ./makeqbk.sh
  COMMAND ${B2_EXE} ${B2_PARAMS}
  BYPRODUCTS "${CMAKE_SOURCE_DIR}/docs/html/index.html"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/docs"
  SOURCES "${doc_srcs}"
  )
set_startup_project(chainsqld)

foreach(target IN LISTS targets)
    target_link_libraries(${target}
      ${OPENSSL_LIBRARIES} ${PROTOBUF_LIBRARIES} ${SANITIZER_LIBRARIES} ${MYSQL_LIBRARIES} intx instructions chainsql_libs)

    link_common_libraries(${target})
endforeach()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(WARNING "chainsqld requires a 64 bit target architecture.\n"
    "The most likely cause of this warning is trying to build chainsqld with a 32-bit OS.")
endif()
