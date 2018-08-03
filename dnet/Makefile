# dnet: Makefile; T11.231-T13.444; $DVS:time$

dfstools = ../dus/programs/dfstools/source
dfslib	 = $(dfstools)/dfslib
dar	 = ../dus/programs/dar/source
ldusinc	 = ../ldus/source/include/ldus

sources = \
	dnet_crypt.c			\
	dnet_database.c			\
	dnet_main.c				\
	dnet_threads.c			\
	dnet_connection.c		\
	dnet_stream.c			\
	dnet_packet.c			\
	dnet_command.c			\
	dnet_log.c				\
	dnet_files.c            \
	dnet_tap.c              \
	$(dfslib)/dfslib_crypt.c	\
	$(dfslib)/dfslib_random.c	\
	$(dfslib)/dfslib_string.c	\
	$(dfstools)/lib/dfsrsa.c	\
	$(dar)/lib/crc_c.c			\


headers = \
	dnet_crypt.h			\
	dnet_database.h			\
	dnet_history.h			\
	dnet_threads.h			\
	dnet_connection.h		\
	dnet_stream.h			\
	dnet_packet.h			\
	dnet_command.h			\
	dnet_log.h				\
	dnet_files.h            \
	dnet_tap.h              \
	dthread.h				\
	$(dfslib)/dfslib_crypt.h		\
	$(dfslib)/dfslib_random.h		\
	$(dfslib)/dfslib_string.h		\
	$(dfstools)/include/dfsrsa.h	\
	$(dar)/include/crc.h			\
	$(ldusinc)/list.h				\


flags = -std=gnu11 -O3 -DDFSTOOLS -g -lpthread -Wall -Werror -Wpedantic -Wmissing-prototypes


all: dnet

dnet: $(sources) $(headers) Makefile
	gcc -o dnet $(sources) $(flags)

clean:
	rm dnet

install: dnet
	sudo cp dnet /usr/local/bin/dnet
	sudo cp dnet /usr/local/sbin/dnet
