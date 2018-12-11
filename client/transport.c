/* транспорт, T13.654-T14.596 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "transport.h"
#include "storage.h"
#include "block.h"
#include "netdb.h"
#include "init.h"
#include "sync.h"
#include "miner.h"
#include "pool.h"
#include "version.h"
#include "../dnet/dnet_main.h"
#include "utils/log.h"
#include "utils/atomic.h"

#define NEW_BLOCK_TTL     5
#define REQUEST_WAIT      64
#define REPLY_ID_PVT_TTL  60

pthread_mutex_t g_transport_mutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_process_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_process_cond   = PTHREAD_COND_INITIALIZER;

time_t g_xdag_last_received = 0;
static void *reply_data;
static void *(*reply_callback)(void *block, void *data) = 0;
static void *reply_connection;
static atomic_uint_least64_t reply_id;
static uint64_t last_reply_id;
static int reply_rcvd;
static uint64_t reply_id_private;
static int64_t reply_result;
static void *xdag_update_rip_thread(void *);

struct xdag_send_data {
	struct xdag_block b;
	void *connection;
};

#define add_main_timestamp(a)   ((a)->main_time = xdag_main_time())

static void *xdag_send_thread(void *arg)
{
	struct xdag_send_data *d = (struct xdag_send_data *)arg;

	d->b.field[0].time = xdag_load_blocks(d->b.field[0].time, d->b.field[0].end_time, d->connection, &dnet_send_xdag_packet);
	d->b.field[0].type = XDAG_FIELD_NONCE | XDAG_MESSAGE_BLOCKS_REPLY << 4;

	memcpy(&d->b.field[2], &g_xdag_stats, sizeof(g_xdag_stats));
	add_main_timestamp((struct xdag_stats*)&d->b.field[2]);

	xdag_netdb_send((uint8_t*)&d->b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
	
	dnet_send_xdag_packet(&d->b, d->connection);
	
	free(d);
	
	return 0;
}

static int process_transport_block(struct xdag_block *received_block, void *connection)
{
	struct xdag_stats *stats = (struct xdag_stats *)&received_block->field[2];
	struct xdag_stats *g = &g_xdag_stats;
	xtime_t start_time = xdag_start_main_time();
	xtime_t current_time = xdag_main_time();

	if(current_time >= start_time && stats->total_nmain <= current_time - start_time + 2) {
		if(stats->main_time <= current_time + 2) {
			if(xdag_diff_gt(stats->max_difficulty, g->max_difficulty))
				g->max_difficulty = stats->max_difficulty;

			if(stats->total_nblocks > g->total_nblocks)
				g->total_nblocks = stats->total_nblocks;

			if(stats->total_nmain > g->total_nmain)
				g->total_nmain = stats->total_nmain;

			if(stats->total_nhosts > g->total_nhosts)
				g->total_nhosts = stats->total_nhosts;
		}
	}

	pthread_mutex_lock(&g_transport_mutex);
	g_xdag_last_received = time(0);
	pthread_mutex_unlock(&g_transport_mutex);

	xdag_netdb_receive((uint8_t*)&received_block->field[2] + sizeof(struct xdag_stats),
		(xdag_type(received_block, 1) == XDAG_MESSAGE_SUMS_REPLY ? 6 : 14) * sizeof(struct xdag_field)
		- sizeof(struct xdag_stats));

	switch(xdag_type(received_block, 1)) {
		case XDAG_MESSAGE_BLOCKS_REQUEST:
		{
			struct xdag_send_data *send_data = (struct xdag_send_data *)malloc(sizeof(struct xdag_send_data));

			if(!send_data) return -1;

			memcpy(&send_data->b, received_block, sizeof(struct xdag_block));

			send_data->connection = connection;

			if(received_block->field[0].end_time - received_block->field[0].time <= REQUEST_BLOCKS_MAX_TIME) {
				xdag_send_thread(send_data);
			}
			else {
				pthread_t t;
				int err = pthread_create(&t, 0, xdag_send_thread, send_data);
				if(err != 0) {
					printf("create xdag_send_thread failed, error : %s\n", strerror(err));
					free(send_data);
					return -1;
				}

				err = pthread_detach(t);
				if(err != 0) {
					printf("detach xdag_send_thread failed, error : %s\n", strerror(err));
					return -1;
				}
			}
			break;
		}

		case XDAG_MESSAGE_BLOCKS_REPLY:
		{
			if(atomic_compare_exchange_strong_explicit_uint_least64(&reply_id, (uint64_t*)received_block->field[1].hash, reply_id_private, memory_order_relaxed, memory_order_relaxed)) {
				pthread_mutex_lock(&g_process_mutex);
				if(last_reply_id != *(uint64_t*)received_block->field[1].hash){
					pthread_mutex_unlock(&g_process_mutex);
					break;
				}
				reply_callback = 0;
				reply_data = 0;
				reply_rcvd = 1;
				reply_result = received_block->field[0].time;
				pthread_cond_signal(&g_process_cond);
				pthread_mutex_unlock(&g_process_mutex);
			}
			break;
		}

		case XDAG_MESSAGE_SUMS_REQUEST:
		{
			received_block->field[0].type = XDAG_FIELD_NONCE | XDAG_MESSAGE_SUMS_REPLY << 4;
			received_block->field[0].time = xdag_load_sums(received_block->field[0].time, received_block->field[0].end_time,
				(struct xdag_storage_sum *)&received_block->field[8]);

			memcpy(&received_block->field[2], &g_xdag_stats, sizeof(g_xdag_stats));

			xdag_netdb_send((uint8_t*)&received_block->field[2] + sizeof(struct xdag_stats),
				6 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));

			dnet_send_xdag_packet(received_block, connection);

			break;
		}

		case XDAG_MESSAGE_SUMS_REPLY:
		{
			if(atomic_compare_exchange_strong_explicit_uint_least64(&reply_id, (uint64_t*)received_block->field[1].hash, reply_id_private, memory_order_relaxed, memory_order_relaxed)) {
				pthread_mutex_lock(&g_process_mutex);
				if(last_reply_id != *(uint64_t*)received_block->field[1].hash){
					pthread_mutex_unlock(&g_process_mutex);
					break;
				}
				if(reply_data) {
					memcpy(reply_data, &received_block->field[8], sizeof(struct xdag_storage_sum) * 16);
					reply_data = 0;
				}
				reply_rcvd = 1;
				reply_result = received_block->field[0].time;
				pthread_cond_signal(&g_process_cond);
				pthread_mutex_unlock(&g_process_mutex);
			}
			break;
		}

		case XDAG_MESSAGE_BLOCK_REQUEST:
		{
			struct xdag_block buf, *blk;
			xtime_t t;
			int64_t pos = xdag_get_block_pos(received_block->field[1].hash, &t, &buf);

			if (pos == -2l) {
				dnet_send_xdag_packet(&buf, connection);
			} else if (pos >= 0 && (blk = xdag_storage_load(received_block->field[1].hash, t, pos, &buf))) {
				dnet_send_xdag_packet(blk, connection);
			}

			break;
		}

		default:
			return -1;
	}

	return 0;
}

static int block_arrive_callback(void *packet, void *connection)
{
	struct xdag_block *received_block = (struct xdag_block *)packet;

	const enum xdag_field_type first_field_type = xdag_type(received_block, 0);
	if(first_field_type == g_block_header_type) {
		xdag_sync_add_block(received_block, connection);
	}
	else if(first_field_type == XDAG_FIELD_NONCE) {
		process_transport_block(received_block, connection);
	}
	else {
		return  -1;
	}

	return 0;
}

static int conn_open_check(uint32_t ip, uint16_t port)
{
	for (int i = 0; i < g_xdag_n_blocked_ips; ++i) {
		if(ip == g_xdag_blocked_ips[i]) {
			return -1;
		}
	}

	for (int i = 0; i < g_xdag_n_white_ips; ++i) {
		if(ip == g_xdag_white_ips[i]) {
			return 0;
		}
	}

	return -1;
}

static void conn_close_notify(void *conn)
{
	if (reply_connection == conn)
		reply_connection = 0;
}

/* external interface */

/* starts the transport system; bindto - ip:port for a socket for external connections
* addr-port_pairs - array of pointers to strings with parameters of other host for connection (ip:port),
* npairs - count of the strings,
* nthreads - number of transporrt threads
*/
int xdag_transport_start(int flags, int nthreads, const char *bindto, int npairs, const char **addr_port_pairs)
{
	const char **argv = malloc((npairs + 7) * sizeof(char *)), *version;
	int argc = 0, i, res;

	if (!argv) return -1;

	argv[argc++] = "dnet";
#if !defined(_WIN32) && !defined(_WIN64)
	if (flags & XDAG_DAEMON) {
		argv[argc++] = "-d";
	}
#endif
	
	if (bindto) {
		argv[argc++] = "-s"; 
		argv[argc++] = bindto;
	}

	if (nthreads >= 0) {
		char buf[16] = {0};
		sprintf(buf, "%u", nthreads);
		argv[argc++] = "-t";
		argv[argc++] = strdup(buf);
	}

	for (i = 0; i < npairs; ++i) {
		argv[argc++] = addr_port_pairs[i];
	}
	argv[argc] = 0;
	
	dnet_set_xdag_callback(block_arrive_callback);
	dnet_connection_open_check = &conn_open_check;
	dnet_connection_close_notify = &conn_close_notify;

	res = dnet_init(argc, (char**)argv);
	if (!res) {
		version = strchr(XDAG_VERSION, '-');
		if (version) dnet_set_self_version(version + 1);
	}

	pthread_t t;
	int err = pthread_create(&t, 0, xdag_update_rip_thread, NULL);
	if(err != 0) {
		printf("create xdag_update_rip_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(t);
	if(err != 0) {
		printf("detach xdag_update_rip_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	return res;
}

/* generates an array with random data */
int xdag_generate_random_array(void *array, unsigned long size)
{
	return dnet_generate_random_array(array, size);
}

static int do_request(int type, xtime_t start_time, xtime_t end_time, void *data,
					  void *(*callback)(void *block, void *data))
{
	struct xdag_block b;
	time_t actual_time;
	struct timespec expire_time = {0};
	int res, ret;
	uint64_t id;

	b.field[0].type = type << 4 | XDAG_FIELD_NONCE;
	b.field[0].time = start_time;
	b.field[0].end_time = end_time;
	
	xdag_generate_random_array(&id, sizeof(uint64_t));

	memset(&b.field[1], 0,  sizeof(struct xdag_field));
	*(uint64_t*)b.field[1].hash = id;
	atomic_exchange_explicit_uint_least64(&reply_id, id, memory_order_acq_rel);

	memcpy(&b.field[2], &g_xdag_stats, sizeof(g_xdag_stats));
	add_main_timestamp((struct xdag_stats*)&b.field[2]);

	xdag_netdb_send((uint8_t*)&b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));

	pthread_mutex_lock(&g_process_mutex);
	last_reply_id = id;
	reply_rcvd = 0;
	reply_result = -1ll;
	reply_data = data;
	reply_callback = callback;
	
	if (type == XDAG_MESSAGE_SUMS_REQUEST) {
		reply_connection = dnet_send_xdag_packet(&b, 0);
		if (!reply_connection) {
			pthread_mutex_unlock(&g_process_mutex);
			return 0;
		}
	} else {
		dnet_send_xdag_packet(&b, reply_connection);
	}

	time(&actual_time);
	expire_time.tv_sec = actual_time + REQUEST_WAIT;

	while(!reply_rcvd){
		if((ret = pthread_cond_timedwait(&g_process_cond, &g_process_mutex, &expire_time))) {
			last_reply_id = reply_id_private;
			reply_data = NULL;
			reply_callback = NULL;
			if(ret != EAGAIN && ret != ETIMEDOUT) {
				xdag_err("pthread_cond_timedwait failed [function: do_request, ret = %d]", ret);
			}
			break;
		}
	}

	res = (int)reply_result;
	pthread_mutex_unlock(&g_process_mutex);

	return res;
}

/* requests all blocks from the remote host, that are in specified time interval;
* calls callback() for each block, callback received the block and data as paramenters;
* return -1 in case of error
*/
int xdag_request_blocks(xtime_t start_time, xtime_t end_time, void *data,
							 void *(*callback)(void *block, void *data))
{
	return do_request(XDAG_MESSAGE_BLOCKS_REQUEST, start_time, end_time, data, callback);
}

/* requests a block from a remote host and places sums of blocks into 'sums' array,
* blocks are filtered by interval from start_time to end_time, splitted to 16 parts;
* end - start should be in form 16^k
* (original russian comment is unclear too) */
int xdag_request_sums(xtime_t start_time, xtime_t end_time, struct xdag_storage_sum sums[16])
{
	return do_request(XDAG_MESSAGE_SUMS_REQUEST, start_time, end_time, sums, 0);
}

/* sends a new block to network */
int xdag_send_new_block(struct xdag_block *b)
{
	if(!g_is_miner) {
		dnet_send_xdag_packet(b, (void*)(uintptr_t)NEW_BLOCK_TTL);
	} else {
		xdag_send_block_via_pool(b);
	}
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
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && dnet_test_connection(conn) < 0) {
		conn = (void*)(uintptr_t)1l;
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
	add_main_timestamp((struct xdag_stats*)&b.field[2]);

	xdag_netdb_send((uint8_t*)&b.field[2] + sizeof(struct xdag_stats),
						 14 * sizeof(struct xdag_field) - sizeof(struct xdag_stats));
	
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && dnet_test_connection(conn) < 0) {
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

/* thread to change reply_id_private after REPLY_ID_PVT_TTL */
static void *xdag_update_rip_thread(void *arg)
{
	time_t last_change_time = 0;
	while(1) {
		if (time(NULL) - last_change_time > REPLY_ID_PVT_TTL) {
			time(&last_change_time);
			xdag_generate_random_array(&reply_id_private, sizeof(uint64_t));
		}
		sleep(60);
	}
	return 0;
}

