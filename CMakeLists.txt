cmake_minimum_required(VERSION 3.10)

project(xdag C CXX)

set(SRCROOT ${CMAKE_CURRENT_DIR})
set(CLIENT_DIR client)
set(DNET_DIR dnet)
set(DFS_TOOLS_DIR dus/programs/dfstools/source)
set(DAR_DIR dus/programs/dar/source)
set(LDUS_INCLUDE ldus/source/include/ldus)
set(UTILS_DIR client/utils)
set(JSON_RPC_DIR client/json-rpc)
set(HTTP_DIR client/http)
set(MOVE_STATISTICS_DIR client/utils/moving_statistics)
set(SECP256K1_DIR secp256k1)

enable_language(ASM)

set(XDAG_SOURCE
        ${CLIENT_DIR}/sha256-mb-x86_64-${BUILD_TAG}.s
        ${CLIENT_DIR}/x86_64cpuid-${BUILD_TAG}.s
        ${CLIENT_DIR}/math.c
        ${CLIENT_DIR}/time.c
        ${CLIENT_DIR}/address.c
        ${CLIENT_DIR}/block.c
        ${CLIENT_DIR}/crypt.c
        ${CLIENT_DIR}/hash.c
        ${CLIENT_DIR}/init.c
        ${CLIENT_DIR}/main.c
        ${CLIENT_DIR}/memory.c
        ${CLIENT_DIR}/miner.c
        ${CLIENT_DIR}/netdb.c
        ${CLIENT_DIR}/pool.c
        ${CLIENT_DIR}/mining_common.c
        ${CLIENT_DIR}/sha256.c
        ${CLIENT_DIR}/storage.c
        ${CLIENT_DIR}/sync.c
        ${CLIENT_DIR}/transport.c
        ${CLIENT_DIR}/wallet.c
        ${CLIENT_DIR}/commands.c
        ${CLIENT_DIR}/terminal.c
        ${CLIENT_DIR}/network.c

        ${DNET_DIR}/dnet_crypt.c
        ${DNET_DIR}/dnet_xdag.c
        ${DFS_TOOLS_DIR}/dfslib/dfslib_crypt.c
        ${DFS_TOOLS_DIR}/dfslib/dfslib_random.c
        ${DFS_TOOLS_DIR}/dfslib/dfslib_string.c
        ${DFS_TOOLS_DIR}/lib/dfsrsa.c
        ${DAR_DIR}/lib/crc_c.c
        ${UTILS_DIR}/log.c
        ${UTILS_DIR}/utils.c
        ${UTILS_DIR}/linenoise.c
        ${UTILS_DIR}/dirname.c
        ${MOVE_STATISTICS_DIR}/moving_average.c
        ${JSON_RPC_DIR}/cJSON.c
        ${JSON_RPC_DIR}/cJSON_Utils.c
        ${JSON_RPC_DIR}/rpc_procedures.c
        ${JSON_RPC_DIR}/rpc_procedure.c
        ${JSON_RPC_DIR}/rpc_service.c
        ${JSON_RPC_DIR}/rpc_wrapper.c
        ${JSON_RPC_DIR}/rpc_commands.c
        ${HTTP_DIR}/url.c
        ${HTTP_DIR}/http.c
        #${SECP256K1_DIR}/src/secp256k1.c
    )

include_directories(
        $(SRCROOT)
        ${DNET_DIR}
        ${DFS_LIB_DIR}
        $(DFS_TOOLS_DIR)/include
        $(DAR_DIR)
        $(LDUS_INCLUDE)
        ${UTILS_DIR}
        ${MOVE_STATISTICS_DIR}
        ${JSON_RPC_DIR}
        ${HTTP_DIR}
        ${SECP256K1_DIR}/src
        ${SECP256K1_DIR}/
    )

find_package(OpenSSL REQUIRED)
if( OpenSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "error, OpenSSL not found")
endif()

add_definitions("-DDFSTOOLS -DCHEATCOIN -DNDEBUG -DSHA256_USE_OPENSSL_TXFM -DSHA256_OPENSSL_MBLOCK")
set(CMAKE_C_FLAGS "-std=gnu11 -O3 -g -Wall -Wmissing-prototypes -Wno-unused-result -Wunused-command-line-argument -Wunused-const-variable")
add_executable(xdag ${XDAG_SOURCE} )
target_link_libraries(xdag ${OPENSSL_LIBRARIES})




