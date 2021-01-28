#[===================================================================[
   read version from source
#]===================================================================]

file (STRINGS src/ripple/protocol/impl/BuildInfo.cpp BUILD_INFO)
foreach (line_ ${BUILD_INFO})
  if (line_ MATCHES "versionString[ ]*=[ ]*\"(.+)\"")
    set (chainsqld_version ${CMAKE_MATCH_1})
  endif ()
endforeach ()
if (chainsqld_version)
  message (STATUS "chainsqld version: ${chainsqld_version}")
else ()
  message (FATAL_ERROR "unable to determine chainsqld version")
endif ()
