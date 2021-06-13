cmake_minimum_required (VERSION 3.10)
project (disqueue VERSION 0.0.1)

list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

find_package(LibEvent REQUIRED)

add_executable (disqueue
  src/main.c
  )

if (WIN32)
  target_link_libraries (disqueue ws2_32 Iphlpapi)
endif ()

target_include_directories (disqueue PUBLIC ${LIBEVENT_INCLUDE_DIR})
target_link_libraries (disqueue ${LIBEVENT_LIB})
