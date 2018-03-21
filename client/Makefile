# cheatcoin: Makefile; T13.656-T13.844; $DVS:time$

dnet	 = ../dnet
dfstools = ../dus/programs/dfstools/source
dfslib	 = $(dfstools)/dfslib
dar		 = ../dus/programs/dar/source
ldusinc	 = ../ldus/source/include/ldus
utils    = ../utils

sources = \
	address.c					\
	block.c						\
	crypt.c						\
	hash.c						\
	init.c						\
	log.c						\
	main.c						\
	memory.c					\
	netdb.c						\
	pool.c						\
	sha256.c					\
	sha256-mb-x86_64.s			\
	storage.c					\
	sync.c						\
	transport.c					\
	wallet.c					\
	commands.c					\
	terminal.c					\
	x86_64cpuid.s				\
	$(dnet)/dnet_crypt.c		\
	$(dnet)/dnet_database.c		\
	$(dnet)/dnet_main.c			\
	$(dnet)/dnet_threads.c		\
	$(dnet)/dnet_connection.c	\
	$(dnet)/dnet_stream.c		\
	$(dnet)/dnet_packet.c		\
	$(dnet)/dnet_command.c		\
	$(dnet)/dnet_log.c			\
	$(dnet)/dnet_files.c		\
	$(dnet)/dnet_tap.c			\
	$(dfslib)/dfslib_crypt.c	\
	$(dfslib)/dfslib_random.c	\
	$(dfslib)/dfslib_string.c	\
	$(dfstools)/lib/dfsrsa.c	\
	$(dar)/lib/crc_c.c			\
	$(utils)/utils.c			\


headers = \
	address.h						\
	block.h							\
	crypt.h							\
	hash.h							\
	log.h							\
	init.h							\
	memory.h						\
	netdb.h							\
	pool.h							\
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


flags = -std=gnu11 -O3 -DDFSTOOLS -DCHEATCOIN -DNDEBUG -g -lpthread -lcrypto -lssl -lm -Wall -Wmissing-prototypes -Wno-unused-result -Wl,--export-dynamic


all: xdag

xdag: $(sources) $(headers) Makefile
	gcc -o xdag $(sources) -DSHA256_USE_OPENSSL_TXFM -DSHA256_OPENSSL_MBLOCK $(flags)

clean:
	rm xdag

install: xdag
	sudo cp xdag /usr/local/bin/xdag
	sudo cp xdag /usr/local/sbin/xdag
