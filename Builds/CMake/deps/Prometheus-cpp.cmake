#[===================================================================[
   NIH dep: prometheus-cpp
#]===================================================================]

#set(prometheus_library_dir "${nih_cache_path}/lib")
#set(prometheus_inlcude_dir "${nih_cache_path}/include/prometheus")

if (static)
  set (Prometheus-cpp_USE_STATIC_LIBS ON)
endif ()

message (STATUS "using local Prometheus-cpp build.")

set(prometheus_lib_pre ${ep_lib_prefix})

ExternalProject_Add(prometheus-cpp
  PREFIX ${nih_cache_path}
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
  GIT_TAG v0.13.0
  CMAKE_ARGS
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_INSTALL_PREFIX=<BINARY_DIR>/_installed_
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
    $<$<BOOL:${unity}>:-DCMAKE_UNITY_BUILD=ON}>
    -DCMAKE_DEBUG_POSTFIX=_d
    $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    # -DBUILD_STATIC_LIBS=ON
    -DBUILD_SHARED_LIBS=OFF
    -DENABLE_PUSH=OFF
    -DENABLE_COMPRESSION=OFF
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
      TEST_COMMAND ""
      # INSTALL_COMMAND
      #   ${CMAKE_COMMAND} -E env --unset=DESTDIR ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
      BUILD_BYPRODUCTS
        <BINARY_DIR>/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-pull${ep_lib_suffix}
        <BINARY_DIR>/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-pull_d${ep_lib_suffix}
        <BINARY_DIR>/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-core${ep_lib_suffix}
        <BINARY_DIR>/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-core_d${ep_lib_suffix}
        <BINARY_DIR>/_installed_/bin/prometheus-cpp-core${CMAKE_EXECUTABLE_SUFFIX}
)

ExternalProject_Get_Property (prometheus-cpp BINARY_DIR)
ExternalProject_Get_Property (prometheus-cpp SOURCE_DIR)
  
if (CMAKE_VERBOSE_MAKEFILE)
  print_ep_logs (prometheus-cpp)
endif ()

exclude_if_included (prometheus-cpp)

if (NOT TARGET prometheus-cpp::libprometheus-cpp-pull)
  add_library (prometheus-cpp::libprometheus-cpp-pull STATIC IMPORTED GLOBAL)
endif ()

file(MAKE_DIRECTORY ${BINARY_DIR}/_installed_/include)

set_target_properties (prometheus-cpp::libprometheus-cpp-pull PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-pull_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-pull${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    ${BINARY_DIR}/_installed_/include)
add_dependencies (prometheus-cpp::libprometheus-cpp-pull prometheus-cpp)
exclude_if_included (prometheus-cpp::libprometheus-cpp-pull)

if (NOT TARGET prometheus-cpp::libprometheus-cpp-core)
  add_library (prometheus-cpp::libprometheus-cpp-core STATIC IMPORTED GLOBAL)
endif ()

set_target_properties (prometheus-cpp::libprometheus-cpp-core PROPERTIES
  IMPORTED_LOCATION_DEBUG
    ${BINARY_DIR}/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-core_d${ep_lib_suffix}
  IMPORTED_LOCATION_RELEASE
    ${BINARY_DIR}/_installed_/${CMAKE_INSTALL_LIBDIR}/${prometheus_lib_pre}prometheus-cpp-core${ep_lib_suffix}
  INTERFACE_INCLUDE_DIRECTORIES
    ${BINARY_DIR}/_installed_/include)
add_dependencies (prometheus-cpp::libprometheus-cpp-core prometheus-cpp)
exclude_if_included (prometheus-cpp::libprometheus-cpp-core)


 # if (NOT TARGET prometheus-cpp::prometheus-cpp-core)
    #add_executable (prometheus-cpp::prometheus-cpp-core IMPORTED)
  #  exclude_if_included (prometheus-cpp::prometheus-cpp-core)
 # endif ()
 # set_target_properties (prometheus-cpp::prometheus-cpp-core PROPERTIES
 #   IMPORTED_LOCATION "${BINARY_DIR}/_installed_/bin/prometheus-cpp-core${CMAKE_EXECUTABLE_SUFFIX}")
 # add_dependencies (prometheus-cpp::prometheus-cpp-core prometheus-cpp)
#else ()
#  if (NOT TARGET prometheus-cpp::prometheus-cpp-core)
#    if (EXISTS "${prometheus-cpp_PROTOC_EXECUTABLE}")
 #     add_executable (prometheus-cpp::prometheus-cpp-core IMPORTED)
 #     set_target_properties (prometheus-cpp::prometheus-cpp-core PROPERTIES
  #      IMPORTED_LOCATION "${prometheus-cpp_PROTOC_EXECUTABLE}")
  #  else ()
   #   message (FATAL_ERROR "prometheus-cpp import failed")
   # endif ()
 # endif ()
#endif ()


target_link_libraries(ripple_libs 
  INTERFACE
  prometheus-cpp::libprometheus-cpp-pull
  prometheus-cpp::libprometheus-cpp-core)