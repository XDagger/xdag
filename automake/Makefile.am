# Makefile.am
# created by cz.worker@gmail.com on 2018-03-29
# modify by reymondtu@gmx.com on 2018-05-23

SRCROOT  = ..
client   = $(SRCROOT)/client
dnet	 = $(SRCROOT)/dnet
dfstools = $(SRCROOT)/dus/programs/dfstools/source
dfslib	 = $(dfstools)/dfslib
dar		 = $(SRCROOT)/dus/programs/dar/source
ldusinc	 = $(SRCROOT)/ldus/source/include/ldus
utils    = $(SRCROOT)/client/utils
jsonrpc  = $(SRCROOT)/client/json-rpc

sources = \
	$(client)/sha256-mb-x86_64$(HOST_SUFFIX).s      \
	$(client)/x86_64cpuid$(HOST_SUFFIX).s           \
	$(client)/address.c                             \
	$(client)/block.c                               \
	$(client)/crypt.c                               \
	$(client)/hash.c                                \
	$(client)/init.c                                \
	$(client)/main.c                                \
	$(client)/memory.c                              \
	$(client)/miner.c                               \
	$(client)/netdb.c                               \
	$(client)/pool.c                                \
	$(client)/mining_common.c                       \
	$(client)/sha256.c                              \
	$(client)/storage.c                             \
	$(client)/sync.c                                \
	$(client)/transport.c                           \
	$(client)/wallet.c                              \
	$(client)/commands.c                            \
	$(client)/terminal.c                            \
	$(dnet)/dnet_crypt.c                            \
	$(dnet)/dnet_database.c                         \
	$(dnet)/dnet_main.c                             \
	$(dnet)/dnet_threads.c                          \
	$(dnet)/dnet_connection.c                       \
	$(dnet)/dnet_stream.c                           \
	$(dnet)/dnet_packet.c                           \
	$(dnet)/dnet_command.c                          \
	$(dnet)/dnet_log.c                              \
	$(dnet)/dnet_files.c                            \
	$(dnet)/dnet_tap.c                              \
	$(dfslib)/dfslib_crypt.c                        \
	$(dfslib)/dfslib_random.c                       \
	$(dfslib)/dfslib_string.c                       \
	$(dfstools)/lib/dfsrsa.c                        \
	$(dar)/lib/crc_c.c                              \
	$(utils)/log.c                                  \
	$(utils)/utils.c                                \
	$(utils)/linenoise.c                            \
	$(jsonrpc)/cJSON.c                              \
	$(jsonrpc)/cJSON_Utils.c                        \
	$(jsonrpc)/rpc_procedures.c                     \
	$(jsonrpc)/rpc_service.c                        \
	$(jsonrpc)/rpc_wrapper.c


headers = \
	$(client)/address.h                             \
	$(client)/block.h                               \
	$(client)/crypt.h                               \
	$(client)/hash.h                                \
	$(client)/init.h                                \
	$(client)/memory.h                              \
	$(client)/netdb.h                               \
	$(client)/pool.h                                \
	$(client)/miner.h                               \
	$(client)/mining_common.h                       \
	$(client)/sha256.h                              \
	$(client)/state.h                               \
	$(client)/storage.h                             \
	$(client)/sync.h                                \
	$(client)/transport.h                           \
	$(client)/wallet.h                              \
	$(client)/commands.h                            \
	$(client)/terminal.h                            \
	$(dnet)/dnet_crypt.h                            \
	$(dnet)/dnet_database.h                         \
	$(dnet)/dnet_history.h                          \
	$(dnet)/dnet_threads.h                          \
	$(dnet)/dnet_connection.h                       \
	$(dnet)/dnet_stream.h                           \
	$(dnet)/dnet_packet.h                           \
	$(dnet)/dnet_command.h                          \
	$(dnet)/dnet_log.h                              \
	$(dnet)/dnet_files.h                            \
	$(dnet)/dnet_tap.h                              \
	$(dnet)/dthread.h                               \
	$(dfslib)/dfslib_crypt.h                        \
	$(dfslib)/dfslib_random.h                       \
	$(dfslib)/dfslib_string.h                       \
	$(dfslib)/dfslib_types.h                        \
	$(dfstools)/include/dfsrsa.h                    \
	$(dar)/include/crc.h                            \
	$(ldusinc)/atomic.h                             \
	$(ldusinc)/list.h                               \
	$(ldusinc)/rbtree.h                             \
	$(utils)/log.h                                  \
	$(utils)/utils.h                                \
	$(utils)/linenoise.h                            \
	$(jsonrpc)/cJSON.h                              \
	$(jsonrpc)/cJSON_Utils.h                        \
	$(jsonrpc)/rpc_procedures.h                     \
	$(jsonrpc)/rpc_service.h                        \
	$(jsonrpc)/rpc_wrapper.h

#program
bin_PROGRAMS = xdag

# macro defines
_DEFS =                                             \
	-DDFSTOOLS                                      \
	-DCHEATCOIN                                     \
	-DNDEBUG                                        \
	-DSHA256_USE_OPENSSL_TXFM                       \
	-DSHA256_OPENSSL_MBLOCK

_INCLUDES =                                         \
	-I$(SRCROOT)                                    \
	-I$(dnet)                                       \
	-I$(dfslib)                                     \
	-I$(dfstools)/include                           \
	-I$(dar)                                        \
	-I$(ldusinc)                                    \
	-I$(utils)                                      \
	-I$(jsonrpc)

xdag_SOURCES = $(sources)
#xdag_LDADD = -lpthread -lcrypto -lssl -lm
xdag_CFLAGS = -std=gnu11 -O3 -g $(_DEFS) $(LIBCRYPTO_CFLAGS) $(_INCLUDES) -Wall -Wmissing-prototypes -Wno-unused-result

