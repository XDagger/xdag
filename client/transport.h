/* транспорт, T13.654-T13.788 $DVS:time$ */

#ifndef CHEATCOIN_TRANSPORT_H
#define CHEATCOIN_TRANSPORT_H

#include <time.h>
#include <stdint.h>
#include "block.h"
#include "storage.h"

enum cheatcoin_transport_flags {
	CHEATCOIN_DAEMON	 = 1,
};

/* starts the transport system; bindto - ip:port for a socket for external connections
 * addr-port_pairs - array of pointers to strings with parameters of other host for connection (ip:port),
 * npairs - count of the strings
 */
extern int cheatcoin_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs);

/* generates an array with random data */
extern int cheatcoin_generate_random_array(void *array, unsigned long size);

/* sends a new block to network */
extern int cheatcoin_send_new_block(struct cheatcoin_block *b);

/* requests all blocks from the remote host, that are in specified time interval;
 * calls callback() for each block, callback recieved the block and data as paramenters;
 * return -1 in case of error
 */
extern int cheatcoin_request_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *, void *));

/* requests a block by hash from another host */
extern int cheatcoin_request_block(cheatcoin_hash_t hash, void *conn);

/* requests a block from a remote host and places sums of blocks into 'sums' array,
 * blocks are filtered by interval from start_time to end_time, splitted to 16 parts;
 * end - start should be in form 16^k
 * (original russian comment is unclear too) */
extern int cheatcoin_request_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* executes transport level command, out - stream to display the result of the command execution */
extern int cheatcoin_net_command(const char *cmd, void *out);

/* sends the package, conn is the same as in function dnet_send_cheatcoin_packet */
extern int cheatcoin_send_packet(struct cheatcoin_block *b, void *conn);

/* see dnet_user_crypt_action */
extern int cheatcoin_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

extern time_t g_cheatcoin_last_received;

#endif
