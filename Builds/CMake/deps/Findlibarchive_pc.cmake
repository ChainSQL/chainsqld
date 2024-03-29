find_package (PkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_search_module (libarchive_PC QUIET libarchive>=3.4.3)
endif ()

if(static)
  set(LIBARCHIVE_LIB libarchive.a)
else()
  set(LIBARCHIVE_LIB archive)
endif()

find_library (archive
  NAMES ${LIBARCHIVE_LIB}
  HINTS
    ${libarchive_PC_LIBDIR}
    ${libarchive_PC_LIBRARY_DIRS}
  NO_DEFAULT_PATH)

find_path (LIBARCHIVE_INCLUDE_DIR
  NAMES archive.h
  HINTS
    ${libarchive_PC_INCLUDEDIR}
    ${libarchive_PC_INCLUDEDIRS}
  NO_DEFAULT_PATH)
