# - Find JSON-c (a json library)
# This module defines
# JSONC_INCLUDE_DIR, where to find json-c headers
# JSONC_LIB, json-c libraries
# JSONC_FOUND, If false, do not try to use json-c

# set(JSONC_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
# foreach(prefix ${JSONC_EXTRA_PREFIXES})
#   list(APPEND JSONC_INCLUDE_PATHS "${prefix}/include")
#   list(APPEND JSONC_LIB_PATHS "${prefix}/lib")
# endforeach()

if(JSONC_ROOT)
  list(APPEND JSONC_INCLUDE_PATHS "${JSONC_ROOT}/include")
  list(APPEND JSONC_LIB_PATHS "${JSONC_ROOT}/lib")
endif()

find_path(JSONC_INCLUDE_DIR json-c/json_object.h PATHS ${JSONC_INCLUDE_PATHS})
find_library(JSONC_LIB NAMES json-c PATHS ${JSONC_LIB_PATHS})

if (JSONC_LIB AND JSONC_INCLUDE_DIR)
  set(JSONC_FOUND TRUE)
  set(JSONC_LIB ${JSONC_LIB})
else ()
  set(JSONC_FOUND FALSE)
endif ()

if (JSONC_FOUND)
  if (NOT JSONC_FIND_QUIETLY)
    message(STATUS "Found json-c: ${JSONC_LIB}")
  endif ()
else ()
  if (JSONC_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find json-c.")
  endif ()
  message(STATUS "json-c NOT found.")
endif ()

mark_as_advanced(
    JSONC_LIB
    JSONC_INCLUDE_DIR
  )
