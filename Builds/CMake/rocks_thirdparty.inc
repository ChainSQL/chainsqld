set (THIRDPARTY_LIBS "")

if(WITH_SNAPPY)
  add_definitions(-DSNAPPY)
  include_directories(${Snappy_INCLUDE_DIRS})
  set (THIRDPARTY_LIBS ${THIRDPARTY_LIBS} ${Snappy_LIBRARIES})
endif()

if(WITH_LZ4)
  add_definitions(-DLZ4)
  include_directories(${lz4_INCLUDE_DIRS})
  set (THIRDPARTY_LIBS ${THIRDPARTY_LIBS} ${lz4_LIBRARIES})
endif()


