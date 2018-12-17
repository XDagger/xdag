cmake_minimum_required(VERSION 3.2)

# Use a different compiler
# http://www.cmake.org/Wiki/CMake_FAQ#How_do_I_use_a_different_compiler.3F
# it must be done before any language is set (before project|enable_language command)
# set(CMAKE_C_COMPILER "gcc-7")
# set(CMAKE_CXX_COMPILER "g++-7")
# usually avoid this by using environment variables:
#   mkdir build-gcc && cd build-gcc && CC=gcc-7 CXX=g++-7 cmake ..

project(daggercoin)
enable_language(C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result")

set(OPENSSL_ROOT_DIR "/opt/openssl/")

set(DAGGER_SOURCES
  client/http/http.c
  client/http/url.c

  client/json-rpc/cJSON_Utils.c
  client/json-rpc/cJSON.c
  client/json-rpc/rpc_commands.c
  client/json-rpc/rpc_procedure.c
  client/json-rpc/rpc_procedures.c
  client/json-rpc/rpc_service.c
  client/json-rpc/rpc_wrapper.c

  client/utils/moving_statistics/moving_average.c
  client/utils/dirname.c
  client/utils/linenoise.c
  client/utils/log.c
  client/utils/utils.c

  client/address.c
  client/block.c
  client/commands.c
  client/crypt.c
  #client/dvstime.c
  client/global.c
  client/hash.c
  client/init.c
  client/main.c
  client/math.c
  client/memory.c
  client/miner.c
  client/mining_common.c
  client/netdb.c
  client/network.c
  client/pool.c
  client/sha256.c
  client/storage.c
  client/sync.c
  client/terminal.c
  client/time.c
  client/transport.c
  client/wallet.c
  client/xdag_config.c

  dnet/dnet_crypt.c
  dnet/dnet_xdag.c

  dus/programs/dfstools/source/dfslib/dfslib_crypt.c
  dus/programs/dfstools/source/dfslib/dfslib_random.c
  dus/programs/dfstools/source/dfslib/dfslib_string.c
  dus/programs/dfstools/source/lib/dfsrsa.c
  dus/programs/dar/source/lib/crc_c.c
)

set(DAGGER_HEADERS
  client/http/http.h
  client/http/url.h

  client/json-rpc/cJSON_Utils.h
  client/json-rpc/cJSON.h
  client/json-rpc/rpc_commands.h
  client/json-rpc/rpc_procedure.h
  client/json-rpc/rpc_procedures.h
  client/json-rpc/rpc_service.h
  client/json-rpc/rpc_wrapper.h

  client/uthash/uthash.h
  client/uthash/utlist.h

  client/utils/moving_statistics/moving_average.h
  client/utils/atomic.h
  client/utils/dirname.h
  client/utils/linenoise.h
  client/utils/log.h
  client/utils/utils.h

  client/address.h
  client/block.h
  client/commands.h
  client/crypt.h
  client/global.h
  client/hash.h
  client/init.h
  client/math.h
  client/memory.h
  client/miner.h
  client/mining_common.h
  client/netdb.h
  client/network.h
  client/pool.h
  client/sha256.h
  client/state.h
  client/storage.h
  client/sync.h
  client/system.h
  client/terminal.h
  client/time.h
  client/transport.h
  client/types.h
  client/version.h
  client/wallet.h
  client/xdag_config.h

  dnet/dnet_crypt.h
  dnet/dnet_history.h
  dnet/dnet_main.h
  dnet/dnet_packet.h
  dnet/system.h

  dus/programs/dfstools/source/dfslib/dfslib_crypt.h
  dus/programs/dfstools/source/dfslib/dfslib_random.h
  dus/programs/dfstools/source/dfslib/dfslib_string.h
  dus/programs/dfstools/source/dfslib/dfslib_types.h
  dus/programs/dfstools/source/include/dfsrsa.h
  dus/programs/dar/source/include/crc.h

  ldus/source/include/ldus/atomic.h
  ldus/source/include/ldus/list.h
  ldus/source/include/ldus/rbtree.h
)

set(SHA256_LINUX_ASM_SOURCES
  client/sha256-mb-x86_64.s
  client/x86_64cpuid.s
)

set(SHA256_MAC_ASM_SOURCES
  client/sha256-mb-x86_64-mac.s
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

# Find and link the pthreads package
find_package(threads REQUIRED)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

# Find and link the openssl package
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})

add_executable(
 cgi_stats
 client/statsdaemon.c
)
add_executable(
 cgi_block
 client/block.cgi.c
)
