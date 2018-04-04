/* транспорт, T13.654-T13.788 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "transport.h"
#include "storage.h"
#include "block.h"
#include "netdb.h"
#include "init.h"
#include "sync.h"
#include "pool.h"
#include "version.h"
#include "../dnet/dnet_main.h"

#define NEW_BLOCK_TTL   5
#define REQUEST_WAIT    64
#define N_CONNS         4096

time_t g_xdag_last_received = 0;
static void *reply_data;
static void *(*reply_callback)(void *block, void *data) = 0;
static void *reply_connection;
static xdag_hash_t reply_id;
static int64_t reply_result;
static void **connections = 0;
static int N_connections = 0;
static pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;

struct xdag_send_data {
	struct xdag_block b;
	void *connection;
};

static int conn_add_rm(void *conn, int addrm)
{
	int b, e, m;

	pthread_mutex_lock(&conn_mutex);
	
	b = 0, e = N_connections;

	while (b < e) {
		m = (b + e) / 2;

		if (connections[m] == conn) {
			if (addrm < 0) {
				if (m < N_connections - 1) {
					memmove(connections + m, connections + m + 1, (N_connections - m - 1) * sizeof(void *));
				}

				N_connections--;
				m = -1;
			}

			pthread_mutex_unlock(&conn_mutex);

			return m;
		}

		if (connections[m] < conn) {
			b = m + 1;
		} else {
			e = m;
		}
	}

	if (addrm > 0 && N_connections < N_CONNS) {
		if (b < N_connections) {
			memmove(connections + b + 1, connections + b, (N_connections - b) * sizeof(void *));
		}

		N_connections++;
		connections[b] = conn;
	} else {
		b = -1;
	}

	pthread_mutex_unlock(&conn_mutex);
	
	return b;
}

static void *xdag_send_thread(void *arg)
{
	struct xdag_send_data *d = (struct xdag_send_data *)arg;

	d->b.field[0].time = xdag_load_blocks(d->b.field[0].time, d->b.field[0].end_time, d->connection, dnet_send_xdag_packet);
	d->b.field[0].type = XDAG_FIELD_NONCE | XDAG_MESSAGE_BLOCKS_REPLY << 4;

	memcpy(&d->b.field[2], &g_xdag_stats, sizeof(g_xdag_stats));
	
	xdag_netdb_send((uint8_t*)&d->b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
	
	dnet_send_xdag_packet(&d->b, d->connection);
	
	free(d);
	
	return 0;
}

static int block_arrive_callback(void *packet, void *connection)
{
	struct xdag_block *b = (struct xdag_block *)packet;
	int res = 0;

	conn_add_rm(connection, 1);

	switch (xdag_type(b, 0)) {
		case XDAG_FIELD_HEAD:
			xdag_sync_add_block(b, connection);
			break;

		case XDAG_FIELD_NONCE: {
			struct xdag_stats *s = (struct xdag_stats *)&b->field[2], *g = &g_xdag_stats;
			xdag_time_t t0 = xdag_start_main_time(), t = xdag_main_time();
			
			if (t < t0 || s->total_nmain > t - t0 + 2) return -1;

			if (xdag_diff_gt(s->max_difficulty, g->max_difficulty))
				g->max_difficulty = s->max_difficulty;
			
			if (s->total_nblocks  > g->total_nblocks)
				g->total_nblocks = s->total_nblocks;
			
			if (s->total_nmain    > g->total_nmain)
				g->total_nmain = s->total_nmain;
			
			if (s->total_nhosts   > g->total_nhosts)
				g->total_nhosts = s->total_nhosts;
			
			g_xdag_last_received = time(0);
			
			xdag_netdb_receive((uint8_t*)&b->field[2] + sizeof(struct xdag_stats),
									(xdag_type(b, 1) == XDAG_MESSAGE_SUMS_REPLY ? 6 : 14) * sizeof(struct xdag_field)
									- sizeof(struct xdag_stats));

			switch (xdag_type(b, 1)) {
				case XDAG_MESSAGE_BLOCKS_REQUEST: {
					struct xdag_send_data *d = (struct xdag_send_data *)malloc(sizeof(struct xdag_send_data));
					
					if (!d) return -1;
					
					memcpy(&d->b, b, sizeof(struct xdag_block));
					
					d->connection = connection;
					
					if (b->field[0].end_time - b->field[0].time <= REQUEST_BLOCKS_MAX_TIME) {
						xdag_send_thread(d);
					} else {
						pthread_t t;
						
						if (pthread_create(&t, 0, xdag_send_thread, d) < 0) {
							free(d); return -1;
						}

						pthread_detach(t);
					}
					break;
				}

				case XDAG_MESSAGE_BLOCKS_REPLY:
					if (!memcmp(b->field[1].hash, reply_id, sizeof(xdag_hash_t))) {
						reply_callback = 0;
						reply_data = 0;
						reply_result = b->field[0].time;
					}
					break;

				case XDAG_MESSAGE_SUMS_REQUEST:
					b->field[0].type = XDAG_FIELD_NONCE | XDAG_MESSAGE_SUMS_REPLY << 4;
					b->field[0].time = xdag_load_sums(b->field[0].time, b->field[0].end_time,
														   (struct xdag_storage_sum *)&b->field[8]);
					
					memcpy(&b->field[2], &g_xdag_stats, sizeof(g_xdag_stats));
					
					xdag_netdb_send((uint8_t*)&b->field[2] + sizeof(struct xdag_stats),
										 6 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
					
					dnet_send_xdag_packet(b, connection);
					
					break;

				case XDAG_MESSAGE_SUMS_REPLY:
					if (!memcmp(b->field[1].hash, reply_id, sizeof(xdag_hash_t))) {
						if (reply_data) {
							memcpy(reply_data, &b->field[8], sizeof(struct xdag_storage_sum) * 16);
							reply_data = 0;
						}
						reply_result = b->field[0].time;
					}
					break;

				case XDAG_MESSAGE_BLOCK_REQUEST: {
					struct xdag_block buf, *blk;
					xdag_time_t t;
					int64_t pos = xdag_get_block_pos(b->field[1].hash, &t);
					
					if (pos >= 0 && (blk = xdag_storage_load(b->field[1].hash, t, pos, &buf)))
						dnet_send_xdag_packet(blk, connection);

					break;
				}
				
				default:
					return -1;
			}
			
			break;
		}
		
		default:
			return -1;
	}

	return res;
}

static int conn_open_check(void *conn, uint32_t ip, uint16_t port)
{
	int i;

	for (i = 0; i < g_xdag_n_blocked_ips; ++i) {
		if (ip == g_xdag_blocked_ips[i]) return -1;
	}

	for (i = 0; i < g_xdag_n_white_ips; ++i) {
		if (ip == g_xdag_white_ips[i]) return 0;
	}

	return -1;
}

static void conn_close_notify(void *conn)
{
	conn_add_rm(conn, -1);

	if (reply_connection == conn)
		reply_connection = 0;
}

/* external interface */

/* starts the transport system; bindto - ip:port for a socket for external connections
* addr-port_pairs - array of pointers to strings with parameters of other host for connection (ip:port),
* npairs - count of the strings
*/
int xdag_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs)
{
	const char **argv = malloc((npairs + 5) * sizeof(char *)), *version;
	int argc = 0, i, res;

	if (!argv) return -1;

	argv[argc++] = "dnet";
#if !defined(_WIN32) && !defined(_WIN64)
	if (flags & XDAG_DAEMON) {
		argv[argc++] = "-d";
	}
#endif
	
	if (bindto) {
		argv[argc++] = "-s"; argv[argc++] = bindto;
	}

	for (i = 0; i < npairs; ++i) {
		argv[argc++] = addr_port_pairs[i];
	}
	argv[argc] = 0;
	
	dnet_set_xdag_callback(block_arrive_callback);
	dnet_connection_open_check = &conn_open_check;
	dnet_connection_close_notify = &conn_close_notify;
	
	connections = (void**)malloc(N_CONNS * sizeof(void *));
	if (!connections) return -1;
	
	res = dnet_init(argc, (char**)argv);
	if (!res) {
		version = strchr(XDAG_VERSION, '-');
		if (version) dnet_set_self_version(version + 1);
	}
	
	return res;
}

/* generates an array with random data */
int xdag_generate_random_array(void *array, unsigned long size)
{
	return dnet_generate_random_array(array, size);
}

static int do_request(int type, xdag_time_t start_time, xdag_time_t end_time, void *data,
					  void *(*callback)(void *block, void *data))
{
	struct xdag_block b;
	time_t t;

	b.field[0].type = type << 4 | XDAG_FIELD_NONCE;
	b.field[0].time = start_time;
	b.field[0].end_time = end_time;
	
	xdag_generate_random_array(&b.field[1], sizeof(struct xdag_field));
	
	memcpy(&reply_id, &b.field[1], sizeof(struct xdag_field));
	memcpy(&b.field[2], &g_xdag_stats, sizeof(g_xdag_stats));
	
	xdag_netdb_send((uint8_t*)&b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
	
	reply_result = -1ll;
	reply_data = data;
	reply_callback = callback;
	
	if (type == XDAG_MESSAGE_SUMS_REQUEST) {
		reply_connection = dnet_send_xdag_packet(&b, 0);
		if (!reply_connection) return 0;
	} else {
		dnet_send_xdag_packet(&b, reply_connection);
	}

	for (t = time(0); reply_result < 0 && time(0) - t < REQUEST_WAIT; ) {
		sleep(1);
	}
	
	return (int)reply_result;
}

/* requests all blocks from the remote host, that are in specified time interval;
* calls callback() for each block, callback recieved the block and data as paramenters;
* return -1 in case of error
*/
int xdag_request_blocks(xdag_time_t start_time, xdag_time_t end_time, void *data,
							 void *(*callback)(void *block, void *data))
{
	return do_request(XDAG_MESSAGE_BLOCKS_REQUEST, start_time, end_time, data, callback);
}

/* requests a block from a remote host and places sums of blocks into 'sums' array,
* blocks are filtered by interval from start_time to end_time, splitted to 16 parts;
* end - start should be in form 16^k
* (original russian comment is unclear too) */
int xdag_request_sums(xdag_time_t start_time, xdag_time_t end_time, struct xdag_storage_sum sums[16])
{
	return do_request(XDAG_MESSAGE_SUMS_REQUEST, start_time, end_time, sums, 0);
}

/* sends a new block to network */
int xdag_send_new_block(struct xdag_block *b)
{
	dnet_send_xdag_packet(b, (void*)(uintptr_t)NEW_BLOCK_TTL);
	xdag_send_block_via_pool(b);

	return 0;
}

/* executes transport level command, out - stream to display the result of the command execution */
int xdag_net_command(const char *cmd, void *out)
{
	return dnet_execute_command(cmd, out);
}

/* sends the package, conn is the same as in function dnet_send_xdag_packet */
int xdag_send_packet(struct xdag_block *b, void *conn)
{
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && conn_add_rm(conn, 0) < 0) {
		conn = (void*)1l;
	}

	dnet_send_xdag_packet(b, conn);
	
	return 0;
}

/* requests a block by hash from another host */
int xdag_request_block(xdag_hash_t hash, void *conn)
{
	struct xdag_block b;

	b.field[0].type = XDAG_MESSAGE_BLOCK_REQUEST << 4 | XDAG_FIELD_NONCE;
	b.field[0].time = b.field[0].end_time = 0;

	memcpy(&b.field[1], hash, sizeof(xdag_hash_t));
	memcpy(&b.field[2], &g_xdag_stats, sizeof(g_xdag_stats));
	
	xdag_netdb_send((uint8_t*)&b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
	
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && conn_add_rm(conn, 0) < 0) {
		conn = (void*)(uintptr_t)1l;
	}

	dnet_send_xdag_packet(&b, conn);
	
	return 0;
}

/* see dnet_user_crypt_action */
int xdag_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action)
{
	return dnet_user_crypt_action(data, data_id, size, action);
}
