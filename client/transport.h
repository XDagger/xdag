/* транспорт, T13.654-T13.788 $DVS:time$ */

#ifndef XDAG_TRANSPORT_H
#define XDAG_TRANSPORT_H

#include <time.h>
#include <stdint.h>
#include "block.h"
#include "storage.h"

enum xdag_transport_flags {
	XDAG_DAEMON = 1,
};

/* starts the transport system; bindto - ip:port for a socket for external connections
 * addr-port_pairs - array of pointers to strings with parameters of other host for connection (ip:port),
 * npairs - count of the strings
 */
extern int xdag_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs);

/* generates an array with random data */
extern int xdag_generate_random_array(void *array, unsigned long size);

/* sends a new block to network */
extern int xdag_send_new_block(struct xdag_block *b);

/* requests all blocks from the remote host, that are in specified time interval;
 * calls callback() for each block, callback recieved the block and data as paramenters;
 * return -1 in case of error
 */
extern int xdag_request_blocks(xdag_time_t start_time, xdag_time_t end_time, void *data,
									void *(*callback)(void *, void *));

/* requests a block by hash from another host */
extern int xdag_request_block(xdag_hash_t hash, void *conn);

/* requests a block from a remote host and places sums of blocks into 'sums' array,
 * blocks are filtered by interval from start_time to end_time, splitted to 16 parts;
 * end - start should be in form 16^k
 * (original russian comment is unclear too) */
extern int xdag_request_sums(xdag_time_t start_time, xdag_time_t end_time, struct xdag_storage_sum sums[16]);

/* executes transport level command, out - stream to display the result of the command execution */
extern int xdag_net_command(const char *cmd, void *out);

/* sends the package, conn is the same as in function dnet_send_xdag_packet */
extern int xdag_send_packet(struct xdag_block *b, void *conn);

/* see dnet_user_crypt_action */
extern int xdag_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

extern time_t g_xdag_last_received;

#endif
