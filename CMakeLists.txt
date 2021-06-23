cmake_minimum_required (VERSION 3.10)
project (disqueue VERSION 0.0.1)

list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

include (CheckIncludeFile)

find_package (LibEvent REQUIRED)
find_package (Wslay REQUIRED)
find_package (OpenSSL REQUIRED)

CHECK_INCLUDE_FILE (sys/queue.h HAVE_SYS_QUEUE)
configure_file (src/queue-compat.h.in queue-compat.h)

add_executable (disqueue
  src/main.c
  src/ws.c
  )

if (WIN32)
  target_link_libraries (disqueue
    ws2_32
    iphlpapi
    crypt32)
endif ()

target_include_directories (disqueue PUBLIC
  ${LIBEVENT_INCLUDE_DIR}
  ${WSLAY_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  "${PROJECT_BINARY_DIR}")
target_link_libraries (disqueue
  ${LIBEVENT_LIB}
  ${WSLAY_LIB}
  ${OPENSSL_LIBRARIES})
