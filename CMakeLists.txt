cmake_minimum_required(VERSION 3.10)
project(xdag C CXX)
enable_language(C ASM)

#[1]set c/c++ standard and compile flags & definitions
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O0 -g -Wall -Wmissing-prototypes -Wno-unused-result -Wno-unused-function -Wno-unused-variable")
#set gnu compiler c flags
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_C_FLAGS "-std=gnu11 -O0 -g -Wall -Wmissing-prototypes -Wno-unused-result -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable")
endif()

OPTION(DEBUG "Build the project using debugging code" OFF)
OPTION(BUILD_TEST "Build Test Cases" OFF)
OPTION(RUN_TEST "Run Test Cases" OFF)

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
#[1]end

#[2]download and compile external project start
include(cmake/RocksDBConfig.cmake)
include(cmake/Secp256k1Config.cmake)
include(cmake/OpenSSLConfig.cmake)
include(cmake/TCMallocConfig.cmake)
#[2]end

#[3]download and compile projects in hunter
find_package(RocksDB CONFIG)
if(ROCKSDB_FOUND)
  message("-- Found RocksDB: include ${RocksDB_INCLUDE_DIR}  libraries ${RocksDB_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find RocksDB dependency!")
endif()

find_package(Secp256k1 CONFIG)
if(SECP256K1_FOUND)
  message("-- Found Secp256k1: include ${SECP256K1_INCLUDE_DIR}  libraries ${SECP256K1_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find SECP256K1 dependency!")
endif()

find_package(OpenSSL CONFIG)
if(OPENSSL_FOUND)
  message("-- Found OPENSSL: include ${OPENSSL_INCLUDE_DIR}  libraries ${OPENSSL_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find OPENSSL dependency!")
endif()

find_package(TCMalloc CONFIG)
if(TCMALLOC_FOUND)
  message("-- Found TCMalloc: include ${TCMALLOC_INCLUDE_DIR}  libraries ${TCMALLOC_LIBRARY} ${PROFILER_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find TCMalloc dependency!")
endif()

find_package(TCMalloc CONFIG)
if(TCMALLOC_FOUND)
  message("-- Found TCMalloc: include ${TCMALLOC_INCLUDE_DIR}  libraries ${TCMALLOC_LIBRARY} ${PROFILER_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find TCMalloc dependency!")
endif()
#[3]end

#[4]find original libraries in system start
find_package(Threads QUIET)
if(NOT Threads_FOUND)
  message(FATAL_ERROR "Failed to find pthreads dependency!")
endif()
#[4]end

#[5]include directories of external libraries
include_directories(${RocksDB_INCLUDE_DIR})
#[5]end

#[6]add source file start
file(GLOB_RECURSE DAGGER_SOURCES
        client/*.c
        dfslib/*.c
        dnet/*.c
        ldus/*.c
        )
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
#[6]end

#[7]ignore sources that we do not intend to include when building the executable...
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*cgi.c$")
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*dvstime.c$")
list(FILTER DAGGER_SOURCES EXCLUDE REGEX ".*statsdaemon.c$")
#[7]end

#[8]compile executable object start
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

add_executable(
        cgi_stats
        client/statsdaemon.c
)

add_executable(
        cgi_block
        client/block.cgi.c
)
#[8]end

#[9]link libraries start
target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        ${RocksDB_INCLUDE_DIR}
        ${SECP256K1_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
        ${TCMALLOC_INCLUDE_DIR}
)
target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        ${RocksDB_LIBRARY}
        ${SECP256K1_LIBRARY}
        ${OPENSSL_SSL_LIBRARY}
        ${OPENSSL_CRYPTO_LIBRARY}
        ${TCMALLOC_LIBRARY}
        ${PROFILER_LIBRARY}
        m
	    dl
)
#[9]end

#[10]build test case
if(BUILD_TEST)
  enable_testing()
  include(cmake/GTestConfig.cmake)
  find_package(GTest CONFIG)
  if(GTEST_FOUND)
    message("-- Found GTest: include ${GTEST_INCLUDE_DIR}  libraries ${GTEST_LIBRARY}")
    add_subdirectory(test)
  else()
    message(FATAL_ERROR "Failed to find GTEST dependency!")
  endif()
endif()
#[10]end