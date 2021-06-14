# - Find Wslay (a websocket library)
# This module defines
# WSLAY_INCLUDE_DIR, where to find Wslay headers
# WSLAY_LIB, Wslay libraries
# Wslay_FOUND, If false, do not try to use wslay

# set(Wslay_EXTRA_PREFIXES /usr/local /opt/local "$ENV{HOME}")
# foreach(prefix ${Wslay_EXTRA_PREFIXES})
#   list(APPEND Wslay_INCLUDE_PATHS "${prefix}/include")
#   list(APPEND Wslay_LIB_PATHS "${prefix}/lib")
# endforeach()

if(WSLAY_ROOT)
  list(APPEND Wslay_INCLUDE_PATHS "${WSLAY_ROOT}/include")
  list(APPEND Wslay_LIB_PATHS "${WSLAY_ROOT}/lib")
endif()

find_path(WSLAY_INCLUDE_DIR wslay/wslay.h PATHS ${Wslay_INCLUDE_PATHS})
find_library(WSLAY_LIB NAMES wslay PATHS ${Wslay_LIB_PATHS})

message(STATUS "${WSLAY_ROOT} ${WSLAY_INCLUDE_DIR} ${WSLAY_LIB}")

if (WSLAY_LIB AND WSLAY_INCLUDE_DIR)
  set(Wslay_FOUND TRUE)
  set(WSLAY_LIB ${WSLAY_LIB})
else ()
  set(Wslay_FOUND FALSE)
endif ()

if (Wslay_FOUND)
  if (NOT Wslay_FIND_QUIETLY)
    message(STATUS "Found wslay: ${WSLAY_LIB}")
  endif ()
else ()
  if (Wslay_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find wslay.")
  endif ()
  message(STATUS "wslay NOT found.")
endif ()

mark_as_advanced(
    WSLAY_LIB
    WSLAY_INCLUDE_DIR
  )
