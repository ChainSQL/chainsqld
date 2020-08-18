# intx: extended precision integer library.
# Copyright 2019 Pawel Bylica.
# Licensed under the Apache License, Version 2.0.

find_path(GMP_INCLUDE_DIR NAMES gmp.h)

# Find the library file, prefer static over dynamic.
find_library(GMP_LIBRARY NAMES ${CMAKE_STATIC_LIBRARY_PREFIX}gmp${CMAKE_STATIC_LIBRARY_SUFFIX} gmp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_LIBRARY GMP_INCLUDE_DIR)
mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)

add_library(GMP::gmp IMPORTED SHARED)
set_property(TARGET GMP::gmp PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET GMP::gmp PROPERTY IMPORTED_LOCATION_RELEASE ${GMP_LIBRARY})
set_property(TARGET GMP::gmp PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${GMP_INCLUDE_DIR})
