/* транспорт, T13.654-T13.788 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "transport.h"
#include "storage.h"
#include "block.h"
#include "netdb.h"
#include "main.h"
#include "sync.h"
#include "pool.h"
#include "version.h"
#include "../dnet/dnet_main.h"

#define NEW_BLOCK_TTL	5
#define REQUEST_WAIT	64
#define N_CONNS			4096

time_t g_cheatcoin_last_received = 0;
static void *reply_data;
static void *(*reply_callback)(void *block, void *data) = 0;
static void *reply_connection;
static cheatcoin_hash_t reply_id;
static int64_t reply_result;
static void **connections = 0;
static int N_connections = 0;
static pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;

struct cheatcoin_send_data {
	struct cheatcoin_block b;
	void *connection;
};

static int conn_add_rm(void *conn, int addrm) {
	int b, e, m;
	pthread_mutex_lock(&conn_mutex);
	b = 0, e = N_connections;
	while (b < e) {
		m = (b + e) / 2;
		if (connections[m] == conn) {
			if (addrm < 0) {
				if (m < N_connections - 1) memmove(connections + m, connections + m + 1, (N_connections - m - 1) * sizeof(void *));
				N_connections--;
				m = -1;
			}
			pthread_mutex_unlock(&conn_mutex);
			return m;
		}
		if (connections[m] < conn) b = m + 1;
		else e = m;
	}
	if (addrm > 0 && N_connections < N_CONNS) {
		if (b < N_connections) memmove(connections + b + 1, connections + b, (N_connections - b) * sizeof(void *));
		N_connections++;
		connections[b] = conn;
	} else b = -1;
	pthread_mutex_unlock(&conn_mutex);
	return b;
}

static void *cheatcoin_send_thread(void *arg) {
	struct cheatcoin_send_data *d = (struct cheatcoin_send_data *)arg;
	d->b.field[0].time = cheatcoin_load_blocks(d->b.field[0].time, d->b.field[0].end_time, d->connection, dnet_send_cheatcoin_packet);
	d->b.field[0].type = CHEATCOIN_FIELD_NONCE | CHEATCOIN_MESSAGE_BLOCKS_REPLY << 4;
	memcpy(&d->b.field[2], &g_cheatcoin_stats, sizeof(g_cheatcoin_stats));
	cheatcoin_netdb_send((uint8_t *)&d->b.field[2] + sizeof(struct cheatcoin_stats),
			14 * sizeof(struct cheatcoin_field) - sizeof(struct cheatcoin_stats));
	dnet_send_cheatcoin_packet(&d->b, d->connection);
	free(d);
	return 0;
}

static int block_arrive_callback(void *packet, void *connection) {
	struct cheatcoin_block *b = (struct cheatcoin_block *)packet;
	int res = 0;
	conn_add_rm(connection, 1);
	switch (cheatcoin_type(b, 0)) {
		case CHEATCOIN_FIELD_HEAD:
			cheatcoin_sync_add_block(b, connection);
			break;
	    case CHEATCOIN_FIELD_NONCE:
			{
			struct cheatcoin_stats *s = (struct cheatcoin_stats *)&b->field[2], *g = &g_cheatcoin_stats;
			cheatcoin_time_t t0 = cheatcoin_start_main_time(), t = cheatcoin_main_time();
			if (t < t0 || s->total_nmain > t - t0 + 2) return -1;
			if (cheatcoin_diff_gt(s->max_difficulty, g->max_difficulty)) g->max_difficulty = s->max_difficulty;
			if (s->total_nblocks  > g->total_nblocks)  g->total_nblocks  = s->total_nblocks;
			if (s->total_nmain    > g->total_nmain)    g->total_nmain    = s->total_nmain;
			if (s->total_nhosts   > g->total_nhosts)   g->total_nhosts   = s->total_nhosts;
			g_cheatcoin_last_received = time(0);
			cheatcoin_netdb_receive((uint8_t *)&b->field[2] + sizeof(struct cheatcoin_stats),
					(cheatcoin_type(b, 1) == CHEATCOIN_MESSAGE_SUMS_REPLY ? 6 : 14) * sizeof(struct cheatcoin_field)
					- sizeof(struct cheatcoin_stats));
			switch (cheatcoin_type(b, 1)) {
				case CHEATCOIN_MESSAGE_BLOCKS_REQUEST:
					{
						struct cheatcoin_send_data *d = (struct cheatcoin_send_data *)malloc(sizeof(struct cheatcoin_send_data));
						if (!d) return -1;
						memcpy(&d->b, b, sizeof(struct cheatcoin_block));
						d->connection = connection;
						if (b->field[0].end_time - b->field[0].time <= REQUEST_BLOCKS_MAX_TIME) {
							cheatcoin_send_thread(d);
						} else {
							pthread_t t;
							if (pthread_create(&t, 0, cheatcoin_send_thread, d) < 0) { free(d); return -1; }
							pthread_detach(t);
						}
					}
					break;
				case CHEATCOIN_MESSAGE_BLOCKS_REPLY:
					if (!memcmp(b->field[1].hash, reply_id, sizeof(cheatcoin_hash_t))) {
						reply_callback = 0;
						reply_data = 0;
						reply_result = b->field[0].time;
					}
					break;
				case CHEATCOIN_MESSAGE_SUMS_REQUEST:
					b->field[0].type = CHEATCOIN_FIELD_NONCE | CHEATCOIN_MESSAGE_SUMS_REPLY << 4;
					b->field[0].time = cheatcoin_load_sums(b->field[0].time, b->field[0].end_time,
							(struct cheatcoin_storage_sum *)&b->field[8]);
					memcpy(&b->field[2], &g_cheatcoin_stats, sizeof(g_cheatcoin_stats));
					cheatcoin_netdb_send((uint8_t *)&b->field[2] + sizeof(struct cheatcoin_stats),
							6 * sizeof(struct cheatcoin_field) - sizeof(struct cheatcoin_stats));
					dnet_send_cheatcoin_packet(b, connection);
					break;
				case CHEATCOIN_MESSAGE_SUMS_REPLY:
					if (!memcmp(b->field[1].hash, reply_id, sizeof(cheatcoin_hash_t))) {
						if (reply_data) {
							memcpy(reply_data, &b->field[8], sizeof(struct cheatcoin_storage_sum) * 16);
							reply_data = 0;
						}
						reply_result = b->field[0].time;
					}
					break;
				case CHEATCOIN_MESSAGE_BLOCK_REQUEST:
					{
						struct cheatcoin_block buf, *blk;
						cheatcoin_time_t t;
						int64_t pos = cheatcoin_get_block_pos(b->field[1].hash, &t);
						if (pos >= 0 && (blk = cheatcoin_storage_load(b->field[1].hash, t, pos, &buf)))
							dnet_send_cheatcoin_packet(blk, connection);
					}
					break;
				default:
					return -1;
			}
			}
			break;
		default:
			return -1;
	}
	return res;
}

static int conn_open_check(void *conn, uint32_t ip, uint16_t port) {
	int i;
	for (i = 0; i < g_cheatcoin_n_blocked_ips; ++i)
		if (ip == g_cheatcoin_blocked_ips[i]) return -1;
	for (i = 0; i < g_cheatcoin_n_white_ips; ++i)
		if (ip == g_cheatcoin_white_ips[i]) return 0;
	return -1;
}

static void conn_close_notify(void *conn) {
	conn_add_rm(conn, -1);
	if (reply_connection == conn) reply_connection = 0;
}

/* внешний интерфейс */

/* запустить транспортную подсистему; bindto - ip:port у которому привязать сокет, принимающий внешние соединения,
 * addr-port_pairs - массив указателей на строки ip:port, содержащие параметры других хостов для подключения; npairs - их число */
int cheatcoin_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs) {
	const char **argv = malloc((npairs + 5) * sizeof(char *)), *version;
	int argc = 0, i, res;
	if (!argv) return -1;
	argv[argc++] = "dnet";
#if !defined(_WIN32) && !defined(_WIN64)
	if (flags & CHEATCOIN_DAEMON) { argv[argc++] = "-d"; }
#endif
	if (bindto) { argv[argc++] = "-s"; argv[argc++] = bindto; }
	for (i = 0; i < npairs; ++i) argv[argc++] = addr_port_pairs[i];
	argv[argc] = 0;
	dnet_set_cheatcoin_callback(block_arrive_callback);
	dnet_connection_open_check = &conn_open_check;
	dnet_connection_close_notify = &conn_close_notify;
	connections = (void **)malloc(N_CONNS * sizeof(void *));
	if (!connections) return -1;
	res = dnet_init(argc, (char **)argv);
	if (!res) {
		version = strchr(CHEATCOIN_VERSION, '-');
		if (version) dnet_set_self_version(version + 1);
	}
	return res;
}

/* сгенерировать массив случайныз данных */
int cheatcoin_generate_random_array(void *array, unsigned long size) {
	return dnet_generate_random_array(array, size);
}

static int do_request(int type, cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *block, void *data)) {
	struct cheatcoin_block b;
	time_t t;
	b.field[0].type = type << 4 | CHEATCOIN_FIELD_NONCE;
	b.field[0].time = start_time;
	b.field[0].end_time = end_time;
	cheatcoin_generate_random_array(&b.field[1], sizeof(struct cheatcoin_field));
	memcpy(&reply_id, &b.field[1], sizeof(struct cheatcoin_field));
	memcpy(&b.field[2], &g_cheatcoin_stats, sizeof(g_cheatcoin_stats));
	cheatcoin_netdb_send((uint8_t *)&b.field[2] + sizeof(struct cheatcoin_stats),
			14 * sizeof(struct cheatcoin_field) - sizeof(struct cheatcoin_stats));
	reply_result = -1ll;
	reply_data = data;
	reply_callback = callback;
	if (type == CHEATCOIN_MESSAGE_SUMS_REQUEST) {
		reply_connection = dnet_send_cheatcoin_packet(&b, 0);
		if (!reply_connection) return 0;
	} else dnet_send_cheatcoin_packet(&b, reply_connection);
	for (t = time(0); reply_result < 0 && time(0) - t < REQUEST_WAIT; ) sleep(1);
	return (int)reply_result;
}

/* запросить у другого хоста все блоки, попадающиев данный временной интервал; для каждого блока вызывается функция
 * callback(), в которую передаётся блок и данные; возвращает -1 в случае ошибки */
int cheatcoin_request_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *block, void *data)) {
	return do_request(CHEATCOIN_MESSAGE_BLOCKS_REQUEST, start_time, end_time, data, callback);
}

/* запрашивает на удалённом хосте и помещает в массив sums суммы блоков по отрезку от start до end, делённому на 16 частей;
 * end - start должно быть вида 16^k */
int cheatcoin_request_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]) {
	return do_request(CHEATCOIN_MESSAGE_SUMS_REQUEST, start_time, end_time, sums, 0);
}

/* разослать другим участникам сети новый блок */
int cheatcoin_send_new_block(struct cheatcoin_block *b) {
	dnet_send_cheatcoin_packet(b, (void *)(uintptr_t)NEW_BLOCK_TTL);
	cheatcoin_send_block_via_pool(b);
	return 0;
}

int cheatcoin_net_command(const char *cmd, void *out) {
	return dnet_execute_command(cmd, out);
}

/* разослать пакет, conn - то же, что и в dnet_send_cheatcoin_packet */
int cheatcoin_send_packet(struct cheatcoin_block *b, void *conn) {
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && conn_add_rm(conn, 0) < 0) conn = (void *)1l;
	dnet_send_cheatcoin_packet(b, conn);
	return 0;
}

/* запросить у другого хоста (по данному соединению) блок по его хешу */
int cheatcoin_request_block(cheatcoin_hash_t hash, void *conn) {
	struct cheatcoin_block b;
	b.field[0].type = CHEATCOIN_MESSAGE_BLOCK_REQUEST << 4 | CHEATCOIN_FIELD_NONCE;
	b.field[0].time = b.field[0].end_time = 0;
	memcpy(&b.field[1], hash, sizeof(cheatcoin_hash_t));
	memcpy(&b.field[2], &g_cheatcoin_stats, sizeof(g_cheatcoin_stats));
	cheatcoin_netdb_send((uint8_t *)&b.field[2] + sizeof(struct cheatcoin_stats),
			14 * sizeof(struct cheatcoin_field) - sizeof(struct cheatcoin_stats));
	if ((uintptr_t)conn & ~0xffl && !((uintptr_t)conn & 1) && conn_add_rm(conn, 0) < 0) conn = (void *)(uintptr_t)1l;
	dnet_send_cheatcoin_packet(&b, conn);
	return 0;
}

/* см. dnet_user_crypt_action */
int cheatcoin_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action) {
	return dnet_user_crypt_action(data, data_id, size, action);
}
