# cheatcoin: Makefile; T13.656-T14.335; $DVS:time$

SRCROOT  		= ..
dnet	 		= ../dnet
dfstools 		= ../dus/programs/dfstools/source
dfslib			= $(dfstools)/dfslib
dar		        = ../dus/programs/dar/source
ldusinc	 		= ../ldus/source/include/ldus
utils    		= ./utils
moving_statistics 	= ./utils/moving_statistics
secp256k1               = ../secp256k1
OS			:= $(shell uname)
use_openssl_ec		:= $(shell if grep --quiet -e 'define USE_OPTIMIZED_EC 1' -e 'define USE_OPTIMIZED_EC 2' crypt.h; then echo false; else echo true; fi)
lgmp_installed		:= $(shell if cc -lgmp 2>&1 | grep --quiet main; then echo true; else echo false; fi)

ifeq ($(OS), Darwin)
	asm_src = 						\
		sha256-mb-x86_64-mac.s				\
		x86_64cpuid-mac.s
else
	asm_src = 						\
		sha256-mb-x86_64.s				\
		x86_64cpuid.s
endif

sources = 							\
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
	mining_common.c						\
	sha256.c						\
	storage.c						\
	sync.c							\
	transport.c						\
	wallet.c						\
	commands.c						\
	terminal.c						\
	network.c						\
	$(dnet)/dnet_crypt.c					\
	$(dnet)/dnet_xdag.c					\
	$(dfslib)/dfslib_crypt.c				\
	$(dfslib)/dfslib_random.c				\
	$(dfslib)/dfslib_string.c				\
	$(dfstools)/lib/dfsrsa.c				\
	$(dar)/lib/crc_c.c					\
	$(utils)/utils.c					\
	$(utils)/log.c						\
	$(utils)/linenoise.c					\
	$(utils)/dirname.c					\
	$(moving_statistics)/moving_average.c			\
	./json-rpc/cJSON.c					\
	./json-rpc/cJSON_Utils.c				\
	./json-rpc/rpc_procedure.c				\
	./json-rpc/rpc_procedures.c				\
	./json-rpc/rpc_service.c				\
	./json-rpc/rpc_wrapper.c				\
	./http/url.c						\
	./http/http.c						\


headers = 							\
	address.h						\
	block.h							\
	crypt.h							\
	hash.h							\
	init.h							\
	memory.h						\
	netdb.h							\
	pool.h							\
	miner.h							\
	mining_common.h						\
	sha256.h						\
	state.h							\
	storage.h						\
	sync.h							\
	transport.h						\
	wallet.h						\
	commands.h						\
	terminal.h						\
	types.h							\
	network.h						\
	$(dnet)/dnet_crypt.h					\
	$(dnet)/dnet_history.h					\
	$(dnet)/dnet_main.h					\
	$(dnet)/dnet_packet.h					\
	$(dfslib)/dfslib_crypt.h				\
	$(dfslib)/dfslib_random.h				\
	$(dfslib)/dfslib_string.h				\
	$(dfslib)/dfslib_types.h				\
	$(dfstools)/include/dfsrsa.h				\
	$(dar)/include/crc.h					\
	$(ldusinc)/atomic.h					\
	$(ldusinc)/list.h					\
	$(ldusinc)/rbtree.h					\
	$(utils)/utils.h					\
	$(utils)/log.h						\
	$(utils)/linenoise.h					\
	$(utils)/dirname.h					\
	$(moving_statistics)/moving_average.h			\
	./json-rpc/cJSON.h					\
	./json-rpc/cJSON_Utils.h				\
	./json-rpc/rpc_procedure.h				\
	./json-rpc/rpc_procedures.h				\
	./json-rpc/rpc_service.h				\
	./json-rpc/rpc_wrapper.h				\
	./http/url.h						\
	./http/http.h						\



ifeq ($(OS), Darwin)
	flags = -std=gnu11 -O3 -DDFSTOOLS -DCHEATCOIN -DNDEBUG -g -lpthread -lcrypto -lssl -lm -Wall -Wmissing-prototypes -Wno-unused-result
else
	flags = -std=gnu11 -O3 -DDFSTOOLS -DCHEATCOIN -DNDEBUG -g -lpthread -lcrypto -lssl -lm -Wall -Wmissing-prototypes -Wno-unused-result -Wl,--export-dynamic
endif

ifneq ($(OS), Darwin)
 ifeq ($(use_openssl_ec), false)
	secp256k1_i = secp256k1.o -isystem$(secp256k1)/include/ $(include_gmp)
  ifeq ("$(wildcard $(secp256k1)/src/libsecp256k1-config.h)","")
	secp256k1_full_compile = true
  endif
  ifeq ("$(wildcard secp256k1.o)","")
	secp256k1_full_compile = true
  endif
  ifeq ($(OS), Linux)
	GNUmake = make
  else
	GNUmake = gmake
  endif
  ifeq ($(lgmp_installed), true)
	include_gmp = -lgmp
  endif
 endif
endif


all: secp256k1 xdag

xdag: $(sources) $(headers) Makefile
	cc -o xdag $(secp256k1_i) $(sources) $(asm_src) -DSHA256_USE_OPENSSL_TXFM -DSHA256_OPENSSL_MBLOCK -I$(SRCROOT) -I$(utils) $(flags)

secp256k1:
ifneq ($(OS), Darwin)
 ifeq ($(use_openssl_ec), false)
  ifeq ($(lgmp_installed), false)
	@echo "\033[0;31mPlease install libgmp-dev to have better performance\033[0m";
  endif
  ifeq ($(secp256k1_full_compile), true)
	cd $(secp256k1) && ./autogen.sh && ./configure && $(GNUmake);										\
	cd ../client && cc -O3 -DHAVE_CONFIG_H -I$(secp256k1)/src/ -I$(secp256k1) -c $(secp256k1)/src/secp256k1.c $(include_gmp)
  else
	if ! cc -O3 -DHAVE_CONFIG_H -I$(secp256k1)/src/ -I$(secp256k1) -c $(secp256k1)/src/secp256k1.c $(include_gmp); then			\
		cd $(secp256k1) && ./autogen.sh && ./configure && $(GNUmake);									\
		cd ../client && cc -O3 -DHAVE_CONFIG_H -I$(secp256k1)/src/ -I$(secp256k1) -c $(secp256k1)/src/secp256k1.c $(include_gmp);	\
	fi
  endif
 else
	@rm -f secp256k1.o
 endif
else
	@rm -f secp256k1.o
endif

clean:
	rm -f xdag
	rm -f secp256k1.o

install: xdag
	sudo cp xdag /usr/local/bin/xdag
	sudo cp xdag /usr/local/sbin/xdag
