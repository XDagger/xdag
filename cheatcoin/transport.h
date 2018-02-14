/* transport, T13.654-T13.788 $ DVS: time $ */

#ifndef CHEATCOIN_TRANSPORT_H
#define CHEATCOIN_TRANSPORT_H

#include <time.h>
#include <stdint.h>
#include "block.h"
#include "storage.h"

enum cheatcoin_transport_flags {
	CHEATCOIN_DAEMON	 = 1,
};

/* start the transport subsystem; bindto - ip: port to bind a socket that accepts external connections, * addr-port_pairs - an array of pointers to the ip: port lines that contain the parameters of other hosts for connection; npairs - their number */
extern int cheatcoin_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs);

/* generate an array of random data */
extern int cheatcoin_generate_random_array(void *array, unsigned long size);

/* send a new block to other network members */
extern int cheatcoin_send_new_block(struct cheatcoin_block *b);

/* request from the other host all the blocks that fall within this time interval; For each block, the * callback () function is invoked, to which the block and data are transferred; returns -1 if an error occurs */
extern int cheatcoin_request_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *, void *));

/* request from another host (for this connection) a block by its hash */
extern int cheatcoin_request_block(cheatcoin_hash_t hash, void *conn);

/* requests on the remote host and puts in the sums array sums of blocks on a line from start to end, divided into 16 parts; * end-start should be of the form 16 ^ k */
extern int cheatcoin_request_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* execute a transport level command, out - a stream to display the result of the command execution */
extern int cheatcoin_net_command(const char *cmd, void *out);

/* send a packet, conn - the same as in dnet_send_cheatcoin_packet */
extern int cheatcoin_send_packet(struct cheatcoin_block *b, void *conn);

/*  см. dnet_user_crypt_action  */
extern int cheatcoin_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

extern time_t g_cheatcoin_last_received;

#endif
