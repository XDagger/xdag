cmake_minimum_required(VERSION 3.10)
include("cmake/HunterGate.cmake")

HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.23.214.tar.gz"
        SHA1 "e14bc153a7f16d6a5eeec845fb0283c8fad8c358"
)

project(xdag C CXX)
enable_language(C ASM)

#[1]set c/c++ standard and compile flags & definitions
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result -Wno-unused-function -Wno-unused-variable")
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
#[1]end

#[2]download and compile external project start
include(cmake/ProjectSecp256k1.cmake)
include(cmake/RocksDBConfig.cmake)
#[2]end

#[3]download and compile projects in hunter
#find_package(RocksDB CONFIG)
if(ROCKSDB_FOUND)
  message("-- Found RocksDB: include ${RocksDB_INCLUDE_DIR}  libraries ${RocksDB_LIBRARY}")
else()
  message(FATAL_ERROR "Failed to find RocksDB dependency!")
endif()

#hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)
#[3]end

#[4]find original libraries in system start
find_package(Threads QUIET)
if(NOT Threads_FOUND)
  message(FATAL_ERROR "Failed to find pthreads dependency!")
endif()
#[4]end

#[5]include directories of external libraries

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
target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        Secp256k1
        ${RocksDB_LIBRARY}
        OpenSSL::SSL
        OpenSSL::Crypto
        m
)
#[9]end
