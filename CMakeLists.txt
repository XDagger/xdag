cmake_minimum_required(VERSION 3.2)
project(xdag)

# Use a different compiler
# http://www.cmake.org/Wiki/CMake_FAQ#How_do_I_use_a_different_compiler.3F
# it must be done before any language is set (before project|enable_language command)
# set(CMAKE_C_COMPILER "gcc-7")
# set(CMAKE_CXX_COMPILER "g++-7")
# usually avoid this by using environment variables:
#   mkdir build-gcc && cd build-gcc && CC=gcc-7 CXX=g++-7 cmake ..

enable_language(C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result")

#set(ENV{PKG_CONFIG_PATH} secp256k1)
if ( NOT DEFINED ENV{SECP256K1_ROOT_DIR})
  find_package(PkgConfig)
  pkg_search_module(SECP256K1 REQUIRED libsecp256k1)
endif()

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" "/usr/local/ssl/*" "/usr/local/Cellar/openssl/*")
  set(OPENSSL_LIBRARIES "/usr/local/opt/openssl/lib" "/usr/local/ssl/*/lib" "/usr/local/Cellar/openssl/*/lib")
endif()

file(GLOB_RECURSE DAGGER_SOURCES
  client/*.c
  dfslib/*.c
  dnet/*.c
  ldus/*.c
)

# ignore sources that we do not intend to include
# when building the executable...
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*cgi.c$")
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*dvstime.c$")
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*statsdaemon.c$")

file(GLOB_RECURSE DAGGER_HEADERS
  client/*.h
  dfslib/*.h
  dnet/*.h
  ldus/*.h
)

set(SHA256_LINUX_ASM_SOURCES
  client/algorithms/sha256-mb-x86_64.s
  client/x86_64cpuid.s
)

set(SHA256_MAC_ASM_SOURCES
  client/algorithms/sha256-mb-x86_64-mac.s
  client/x86_64cpuid-mac.s
)

OPTION(DEBUG "Build the project using debugging code" OFF)
if(DEBUG)
  MESSAGE("Adding Debug flag...")
  SET(CMAKE_BUILD_TYPE Debug)
  MESSAGE("Build type is " ${CMAKE_BUILD_TYPE})
else()
  add_definitions(-DNDEBUG)
endif(DEBUG)

add_definitions(-DDFSTOOLS)
add_definitions(-DSHA256_USE_OPENSSL_TXFM)
add_definitions(-DSHA256_OPENSSL_MBLOCK)

find_package(Threads QUIET)
if(NOT Threads_FOUND)
  message(FATAL_ERROR "Failed to find pthreads dependency!")
endif()

find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
  message(FATAL_ERROR "Failed to find openssl dependency!")
endif()

include_directories(
  ${OPENSSL_INCLUDE_DIR}
  ${SECP256K1_INCLUDE_DIRS}
)

if(APPLE)
  add_executable(
   ${PROJECT_NAME}
   ${DAGGER_HEADERS}
   ${DAGGER_SOURCES}
   ${SHA256_MAC_ASM_SOURCES}
  )
else(UNIX)
  add_executable(
   ${PROJECT_NAME}
   ${DAGGER_HEADERS}
   ${DAGGER_SOURCES}
   ${SHA256_LINUX_ASM_SOURCES}
  )
endif()

target_link_libraries(
  ${PROJECT_NAME}
  ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(
  ${PROJECT_NAME}
  ${OPENSSL_LIBRARIES}
  ${SECP256K1_LIBRARIES}
)

add_executable(
 cgi_stats
 client/statsdaemon.c
)

add_executable(
 cgi_block
 client/block.cgi.c
)
