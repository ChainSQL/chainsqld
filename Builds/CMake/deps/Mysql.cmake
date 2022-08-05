  IF (WIN32)
    SET(MYSQL_NAMES libmysql)
  ELSE()
    SET(MYSQL_NAMES mysqlclient mysqlclient_r)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  ENDIF()

  if (APPLE)
    if(DEFINED ENV{MYSQL_DIR})
      set(MYSQL_INCLUDE_DIR $ENV{MYSQL_DIR}/include/mysql)
      set(MYSQL_LIBRARY_DIR $ENV{MYSQL_DIR}/lib)
      FIND_LIBRARY(MYSQL_LIBRARY
        NAMES ${MYSQL_NAMES}
        PATHS ${MYSQL_LIBRARY_DIR}
        NO_DEFAULT_PATH
      )
    else()
      find_program(HOMEBREW brew)
      if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
        execute_process(COMMAND brew --prefix mysql-client
          OUTPUT_VARIABLE MYSQL_ROOT
          OUTPUT_STRIP_TRAILING_WHITESPACE)

        FIND_PATH(MYSQL_INCLUDE_DIR mysql.h
          ${MYSQL_ROOT}/include/
          ${MYSQL_ROOT}/include/mysql
        )
        FIND_LIBRARY(MYSQL_LIBRARY
          NAMES ${MYSQL_NAMES}
          PATHS ${MYSQL_ROOT}/lib
        )
        get_filename_component(MYSQL_NAME ${MYSQL_LIBRARY} NAME)
        FIND_PATH(MYSQL_LIBRARY_DIR ${MYSQL_NAME}
          /usr/lib /usr/local/lib /usr/local/mysql/lib ${MYSQL_ROOT}/lib
        )
      endif()
    endif()
  endif()

  if (NOT MYSQL_LIBRARY)
      FIND_PATH(MYSQL_INCLUDE_DIR mysql.h
          IF (WIN32 AND DEFINED ENV{MYSQL_DIR})
              $ENV{MYSQL_DIR}/include
          ELSE()
              /usr/local/include/mysql
              /usr/include/mysql
              /usr/local/mysql/include
          ENDIF()
          )


      if(WIN32 AND DEFINED ENV{MYSQL_DIR})
          set(SEARCH_PATHS $ENV{MYSQL_DIR}/lib)
      else()
          set(SEARCH_PATHS 
              /usr/lib 
              /usr/lib/*/ 
              /usr/local 
              /usr/local/lib 
              /usr/local/mysql)
      endif()

      FIND_LIBRARY(MYSQL_LIBRARY
          NAMES ${MYSQL_NAMES}
          PATHS ${SEARCH_PATHS}
          PATH_SUFFIXES mysql
          )

      get_filename_component(MYSQL_NAME ${MYSQL_LIBRARY} NAME)
      FIND_PATH(MYSQL_LIBRARY_DIR ${MYSQL_NAME}
          ${SEARCH_PATHS} 
          )
  endif()


  IF (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
      SET(MYSQL_FOUND TRUE)
      SET(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
  ELSE ()
      SET(MYSQL_FOUND FALSE)
      SET(MYSQL_LIBRARIES)
  ENDIF ()

  IF (NOT MYSQL_FOUND)
      MESSAGE(FATAL_ERROR "Could NOT find MySQL library")
  endif()

  MESSAGE(STATUS "Found MySQL include path: ${MYSQL_INCLUDE_DIR}")
  MESSAGE(STATUS "Found MySQL library path: ${MYSQL_LIBRARY_DIR}")
  MESSAGE(STATUS "Found MySQL library: ${MYSQL_LIBRARY}")
  
  include_directories(${MYSQL_INCLUDE_DIR})
  #link_directories(${MYSQL_LIBRARY_DIR})  
  target_link_libraries (ripple_libs INTERFACE ${MYSQL_LIBRARY})

