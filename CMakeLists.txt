cmake_minimum_required (VERSION 3.12)
project (disqueue VERSION 0.0.1)

list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

include (CheckIncludeFile)
include (CheckFunctionExists)

find_package (LibEvent REQUIRED)
find_package (Wslay REQUIRED)
find_package (OpenSSL REQUIRED)
find_package (JsonC REQUIRED)

CHECK_INCLUDE_FILE (sys/queue.h HAVE_SYS_QUEUE)
CHECK_INCLUDE_FILE (strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE (arpa/inet.h HAVE_ARPA_INET_H)
CHECK_INCLUDE_FILE (winsock2.h HAVE_WINSOCK2_H)
check_function_exists (strcasecmp HAVE_STRCASECMP)
check_function_exists (_stricmp HAVE__STRICMP)
check_function_exists (stricmp HAVE_STRICMP)
check_function_exists (getopt HAVE_GETOPT)

configure_file (src/queue-compat.h.in queue-compat.h)
configure_file (src/strcase.h.in strcase.h)
configure_file (src/hostnet.h.in hostnet.h)
configure_file (src/getopt.h.in getopt.h)

configure_file (src/config.json config.json COPYONLY)

set (EXTRA_SOURCE "")

if (NOT HAVE_GETOPT)
  list (APPEND EXTRA_SOURCE src/getopt.c)
endif ()

add_executable (disqueue
  src/main.c
  src/queue.c
  src/ws.c
  src/manager.c
  src/protocol.c
  src/connection-http.c
  src/connection-ws.c
  src/config.c
  src/ssl.c
  src/auth.c
  src/auth-plaintext.c
  ${EXTRA_SOURCE}
  )

if (WIN32)
  target_link_libraries (disqueue
    ws2_32
    iphlpapi
    crypt32)

  add_compile_definitions (
    _CRT_NONSTDC_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    )
endif ()

target_include_directories (disqueue PUBLIC
  ${LIBEVENT_INCLUDE_DIR}
  ${WSLAY_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${JSONC_INCLUDE_DIR}
  "${PROJECT_BINARY_DIR}")
target_link_libraries (disqueue
  ${LIBEVENT_LIB}
  ${WSLAY_LIB}
  ${OPENSSL_LIBRARIES}
  ${JSONC_LIB})
