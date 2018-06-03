# cheatcoin: Makefile; T13.656-T13.844; $DVS:time$

SRCROOT  = ..
dnet	 = ../dnet
dfstools = ../dus/programs/dfstools/source
dfslib	 = $(dfstools)/dfslib
dar		 = ../dus/programs/dar/source
ldusinc	 = ../ldus/source/include/ldus
utils    = ./utils
OS	:= $(shell uname)

ifeq ($(OS), Darwin)
	asm_src = \
		sha256-mb-x86_64-mac.s		\
		x86_64cpuid-mac.s
else
	asm_src = \
		sha256-mb-x86_64.s			\
		x86_64cpuid.s
endif

sources = \
	address.c						\
	block.c							\
	crypt.c							\
	hash.c							\
	init.c							\
	main.c							\
	memory.c						\
	netdb.c							\
	pool.c							\
	miner.c							\
	mining_common.c					\
	sha256.c						\
	storage.c						\
	sync.c							\
	transport.c						\
	wallet.c						\
	commands.c						\
	terminal.c						\
	$(dnet)/dnet_crypt.c			\
	$(dnet)/dnet_database.c			\
	$(dnet)/dnet_main.c				\
	$(dnet)/dnet_threads.c			\
	$(dnet)/dnet_connection.c		\
	$(dnet)/dnet_stream.c			\
	$(dnet)/dnet_packet.c			\
	$(dnet)/dnet_command.c			\
	$(dnet)/dnet_log.c				\
	$(dnet)/dnet_files.c			\
	$(dnet)/dnet_tap.c				\
	$(dfslib)/dfslib_crypt.c		\
	$(dfslib)/dfslib_random.c		\
	$(dfslib)/dfslib_string.c		\
	$(dfstools)/lib/dfsrsa.c		\
	$(dar)/lib/crc_c.c				\
	$(utils)/utils.c				\
	$(utils)/log.c					\
	$(utils)/linenoise.c			\
	./json-rpc/cJSON.c				\
	./json-rpc/cJSON_Utils.c		\
	./json-rpc/rpc_procedures.c		\
	./json-rpc/rpc_service.c		\
	./json-rpc/rpc_wrapper.c		\
	./http/url.c					\
	./http/http.c					\


headers = \
	address.h						\
	block.h							\
	crypt.h							\
	hash.h							\
	init.h							\
	memory.h						\
	netdb.h							\
	pool.h							\
	miner.h							\
	mining_common.h					\
	sha256.h						\
	state.h							\
	storage.h						\
	sync.h							\
	transport.h						\
	wallet.h						\
	commands.h						\
	terminal.h						\
	$(dnet)/dnet_crypt.h			\
	$(dnet)/dnet_database.h			\
	$(dnet)/dnet_history.h			\
	$(dnet)/dnet_threads.h			\
	$(dnet)/dnet_connection.h		\
	$(dnet)/dnet_stream.h			\
	$(dnet)/dnet_packet.h			\
	$(dnet)/dnet_command.h			\
	$(dnet)/dnet_log.h				\
	$(dnet)/dnet_files.h			\
	$(dnet)/dnet_tap.h				\
	$(dnet)/dthread.h				\
	$(dfslib)/dfslib_crypt.h		\
	$(dfslib)/dfslib_random.h		\
	$(dfslib)/dfslib_string.h		\
	$(dfslib)/dfslib_types.h		\
	$(dfstools)/include/dfsrsa.h	\
	$(dar)/include/crc.h			\
	$(ldusinc)/atomic.h				\
	$(ldusinc)/list.h				\
	$(ldusinc)/rbtree.h				\
	$(utils)/utils.h				\
	$(utils)/log.h					\
	$(utils)/linenoise.h			\
	./json-rpc/cJSON.h				\
	./json-rpc/cJSON_Utils.h		\
	./json-rpc/rpc_procedures.h		\
	./json-rpc/rpc_service.h		\
	./json-rpc/rpc_wrapper.h		\
	./http/url.h					\
	./http/http.h					\



ifeq ($(OS), Darwin)

	flags = -std=gnu11 -O3 -DDFSTOOLS -DCHEATCOIN -DNDEBUG -g -lpthread -lcrypto -lssl -lm -Wall -Wmissing-prototypes -Wno-unused-result
else
	flags = -std=gnu11 -O3 -DDFSTOOLS -DCHEATCOIN -DNDEBUG -g -lpthread -lcrypto -lssl -lm -Wall -Wmissing-prototypes -Wno-unused-result -Wl,--export-dynamic

endif


all: xdag

xdag: $(sources) $(headers) Makefile
	gcc -o xdag $(sources) $(asm_src) -DSHA256_USE_OPENSSL_TXFM -DSHA256_OPENSSL_MBLOCK -I$(SRCROOT) -I$(utils) $(flags)

clean:
	rm xdag

install: xdag
	sudo cp xdag /usr/local/bin/xdag
	sudo cp xdag /usr/local/sbin/xdag
