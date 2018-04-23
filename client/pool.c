// pool logic

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(_WIN32) || defined(_WIN64)
#else
#include <netinet/in.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#endif
#include "block.h"
#include "sync.h"
#include "mining_common.h"
#include "pool.h"
#include "address.h"
#include "commands.h"
#include "storage.h"
#include "transport.h"
#include "wallet.h"
#include "utils/log.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "uthash/utlist.h"

//TODO: why do we need these two definitions?
#define START_MINERS_COUNT     256
#define START_MINERS_IP_COUNT  8

#define HEADER_WORD            0x3fca9e2bu
#define FUND_ADDRESS           "FQglVQtb60vQv2DOWEUL7yh3smtj7g1s"  /* community fund */
#define SHARES_PER_TASK_LIMIT  20                                  /* maximum count of shares per task */

enum miner_state {
	MINER_UNKNOWN = 0,
	MINER_ACTIVE = 1,
	MINER_ARCHIVE = 2,
	MINER_SERVICE = 3
};

struct miner_pool_data {
	struct xdag_field id;
	xdag_time_t task_time;
	double prev_diff;
	uint32_t prev_diff_count;
	double maxdiff[CONFIRMATIONS_COUNT];
	enum miner_state state;
	uint32_t connections_count;

	//uint32_t shares_count;
};

typedef struct miner_list_element {
	struct miner_pool_data miner_data;
	struct miner_list_element *next;
} miner_list_element;

enum connection_state {
	UNKNOWN_ADDRESS = 0,
	ACTIVE_CONNECTION = 1
};

struct connection_pool_data {
	xdag_time_t task_time;
	double prev_diff;
	uint32_t prev_diff_count;
	double maxdiff[CONFIRMATIONS_COUNT];
	uint32_t data[DATA_SIZE];
	uint64_t nfield_in;
	uint64_t nfield_out;
	uint64_t task_index;
	struct xdag_block *block;
	uint32_t ip;
	uint16_t port;
	enum connection_state state;
	uint8_t data_size;
	uint8_t block_size;
	struct pollfd connection_descriptor;
	struct miner_pool_data *miner;
	int balance_sent;
};

typedef struct connection_list_element {
	struct connection_pool_data connection_data;
	struct connection_list_element *next;
} connection_list_element;

struct payment_data {
	xdag_amount_t balance;
	xdag_amount_t pay;
	xdag_amount_t reward;
	xdag_amount_t direct;
	xdag_amount_t fund;
	double sum;
	double prev_sum;
	int reward_index;
};

xdag_hash_t g_xdag_mined_hashes[CONFIRMATIONS_COUNT], g_xdag_mined_nonce[CONFIRMATIONS_COUNT];

static int g_max_miners_count = START_MINERS_COUNT, g_max_miner_ip_count = START_MINERS_IP_COUNT;
static int g_connections_count = 0;
static double g_pool_fee = 0, g_pool_reward = 0, g_pool_direct = 0, g_pool_fund = 0;
static struct xdag_block *g_firstb = 0, *g_lastb = 0;

static struct miner_pool_data g_pool_miner;
static struct miner_pool_data g_fund_miner;
struct pollfd *g_fds;

connection_list_element *g_connection_list_head = NULL;
miner_list_element *g_miner_list_head = NULL;
pthread_mutex_t g_descriptors_mutex = PTHREAD_MUTEX_INITIALIZER;

int pay_miners(xdag_time_t time);

/* initialization of the pool */
int xdag_initialize_pool(const char *pool_arg)
{
	pthread_t th;

	memset(&g_pool_miner, 0, sizeof(struct miner_pool_data));
	memset(&g_fund_miner, 0, sizeof(struct miner_pool_data));

	xdag_get_our_block(g_pool_miner.id.data);
	g_pool_miner.state = MINER_SERVICE;

	g_fds = malloc(MAX_MINERS_COUNT * sizeof(struct pollfd));
	if(!g_fds) return -1;

	int res = pthread_create(&th, 0, pool_net_thread, (void*)pool_arg);
	if(res) return -1;
	pthread_detach(th);

	res = pthread_create(&th, 0, pool_main_thread, 0);
	if(res) return -1;
	pthread_detach(th);

	res = pthread_create(&th, 0, pool_block_thread, 0);
	if(res) return -1;
	pthread_detach(th);

	return 0;
}

/* sets pool parameters */
int xdag_pool_set_config(const char *pool_config)
{
	char buf[0x100], *lasts;

	if(!g_xdag_pool) return -1;
	strcpy(buf, pool_config);

	pool_config = strtok_r(buf, " \t\r\n:", &lasts);

	if(pool_config) {
		int open_max = sysconf(_SC_OPEN_MAX);

		sscanf(pool_config, "%d", &g_max_miners_count);

		if(g_max_miners_count < 0) {
			g_max_miners_count = 0;
			xdag_warn("pool : wrong miner count");
		} else if(g_max_miners_count > MAX_MINERS_COUNT) {
			g_max_miners_count = MAX_MINERS_COUNT;
			xdag_warn("pool : exceed max miners count %d", MAX_MINERS_COUNT);
		} else if(g_max_miners_count > open_max - 64) {
			g_max_miners_count = open_max - 64;
			xdag_warn("pool : exceed max open files %d", open_max - 64);
		}
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%lf", &g_pool_fee);

		g_pool_fee /= 100;

		if(g_pool_fee < 0)
			g_pool_fee = 0;

		if(g_pool_fee > 1)
			g_pool_fee = 1;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%lf", &g_pool_reward);

		g_pool_reward /= 100;

		if(g_pool_reward < 0)
			g_pool_reward = 0;
		if(g_pool_fee + g_pool_reward > 1)
			g_pool_reward = 1 - g_pool_fee;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%lf", &g_pool_direct);

		g_pool_direct /= 100;

		if(g_pool_direct < 0)
			g_pool_direct = 0;
		if(g_pool_fee + g_pool_reward + g_pool_direct > 1)
			g_pool_direct = 1 - g_pool_fee - g_pool_reward;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%d", &g_max_miner_ip_count);

		if(g_max_miner_ip_count <= 0)
			g_max_miner_ip_count = 1;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%lf", &g_pool_fund);

		g_pool_fund /= 100;

		if(g_pool_fund < 0)
			g_pool_fund = 0;
		if(g_pool_fee + g_pool_reward + g_pool_direct + g_pool_fund > 1)
			g_pool_fund = 1 - g_pool_fee - g_pool_reward - g_pool_direct;
	}

	return 0;
}

/* gets pool parameters as a string, 0 - if the pool is disabled */
char *xdag_pool_get_config(char *buf)
{
	if(!g_xdag_pool) return 0;

	sprintf(buf, "%d:%.2lf:%.2lf:%.2lf:%d:%.2lf", g_max_miners_count, g_pool_fee * 100, g_pool_reward * 100,
		g_pool_direct * 100, g_max_miner_ip_count, g_pool_fund * 100);

	return buf;
}

static int open_pool_connection(const char *pool_arg)
{
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	int rcvbufsize = 1024;
	int reuseaddr = 1;
	char buf[0x100];
	char *nextParam;

	// Create a socket
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock == INVALID_SOCKET) {
		xdag_err("pool: cannot create a socket");
		return INVALID_SOCKET;
	}

	if(fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		xdag_err("pool: can't set FD_CLOEXEC flag on socket %d, %s\n", sock, strerror(errno));
	}

	// Fill in the address of server
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, pool_arg);
	pool_arg = strtok_r(buf, " \t\r\n:", &nextParam);
	if(!pool_arg) {
		xdag_err("pool: host is not given");
		return INVALID_SOCKET;
	}

	peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Resolve port
	pool_arg = strtok_r(0, " \t\r\n:", &nextParam);
	if(!pool_arg) {
		xdag_err("pool: port is not given");
		return INVALID_SOCKET;
	}
	peeraddr.sin_port = htons(atoi(pool_arg));

	int res = bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
	if(res) {
		xdag_err("pool: cannot bind a socket (error %d)", res);
		return INVALID_SOCKET;
	}

	// Set the "LINGER" timeout to zero, to close the listen socket
	// immediately at program termination.
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbufsize, sizeof(int));

	pool_arg = strtok_r(0, " \t\r\n", &nextParam);
	if(pool_arg) {
		xdag_pool_set_config(pool_arg);
	}

	return sock;
}

static int connection_can_be_accepted(int sock, struct sockaddr_in *peeraddr)
{
	connection_list_element *elt;
	int count;

	//firstly we check that total count of connection did not exceed max count of connection
	LL_COUNT(g_connection_list_head, elt, count);
	if(count >= g_max_miners_count) {
		return 0;
	}

	//then we check that count of connections with the same IP address did not exceed the limit
	count = 0;
	LL_FOREACH(g_connection_list_head, elt)
	{
		if(elt->connection_data.ip == peeraddr->sin_addr.s_addr) {
			if(++count >= g_max_miner_ip_count) {
				return 0;
			}
		}
	}

	return 1;
}

static void rebuild_descriptors_array()
{
	connection_list_element *elt;
	int index = 0;
	LL_FOREACH(g_connection_list_head, elt)
	{
		memcpy(g_fds + index, &elt->connection_data.connection_descriptor, sizeof(struct pollfd));
		++index;
	}
}

void *pool_net_thread(void *arg)
{
	const char *pool_arg = (const char*)arg;
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	int rcvbufsize = 1024;

	while(!g_xdag_sync_on) {
		sleep(1);
	}

	int sock = open_pool_connection(pool_arg);
	if(sock == INVALID_SOCKET) {
		return 0;
	}

	// Now, listen for a connection
	int res = listen(sock, MAX_MINERS_COUNT);    // "1" is the maximal length of the queue
	if(res) {
		xdag_err("pool: cannot listen");
		return 0;
	}

	for(;;) {
		// Accept a connection (the "accept" command waits for a connection with
		// no timeout limit...)
		int fd = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
		if(fd < 0) {
			xdag_err("pool: cannot accept connection");
			return 0;
		}

		if(!connection_can_be_accepted(sock, &peeraddr)) {
			close(fd);
			continue;
		}

		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbufsize, sizeof(int));

		struct connection_list_element *new_connection = (struct connection_list_element*)malloc(sizeof(connection_list_element));
		memset(new_connection, 0, sizeof(connection_list_element));
		new_connection->connection_data.connection_descriptor.fd = fd;
		new_connection->connection_data.connection_descriptor.events = POLLIN | POLLOUT;
		new_connection->connection_data.connection_descriptor.revents = 0;
		int ip = new_connection->connection_data.ip = peeraddr.sin_addr.s_addr;
		new_connection->connection_data.port = peeraddr.sin_port;

		LL_APPEND(g_connection_list_head, new_connection);

		pthread_mutex_lock(&g_descriptors_mutex);
		++g_connections_count;
		rebuild_descriptors_array();
		pthread_mutex_unlock(&g_descriptors_mutex);

		xdag_info("Pool  : miner %d connected from %u.%u.%u.%u:%u", g_connections_count,
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(new_connection->connection_data.port));
	}

	return 0;
}

static void close_connection(connection_list_element *connection, int index, const char *message)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

	pthread_mutex_lock(&g_descriptors_mutex);
	LL_DELETE(g_connection_list_head, connection);
	--g_connections_count;
	rebuild_descriptors_array();
	pthread_mutex_unlock(&g_descriptors_mutex);

	close(conn_data->connection_descriptor.fd);

	if(conn_data->block) {
		free(conn_data->block);
	}

	if(conn_data->miner) {
		--conn_data->miner->connections_count;
		if(conn_data->miner->connections_count == 0) {
			conn_data->miner->state = MINER_ARCHIVE;
		}
	}

	uint32_t ip = conn_data->ip;
	uint16_t port = conn_data->port;

	free(connection);

	xdag_info("Pool: miner %d disconnected from %u.%u.%u.%u:%u by %s", index,
		ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(port), message);
}

static void calculate_nopaid_shares(struct connection_pool_data *conn_data, struct xdag_pool_task *task, xdag_hash_t hash)
{
	const xdag_time_t task_time = task->task_time;

	if(conn_data->task_time <= task_time) {
		double diff = ((uint64_t*)hash)[2];
		int i = task_time & (CONFIRMATIONS_COUNT - 1);

		diff = ldexp(diff, -64);
		diff += ((uint64_t*)hash)[3];

		if(diff < 1) diff = 1;

		diff = 46 - log(diff);

		if(conn_data->task_time < task_time) {
			conn_data->task_time = task_time;

			if(conn_data->maxdiff[i] > 0) {
				conn_data->prev_diff += conn_data->maxdiff[i];
				conn_data->prev_diff_count++;
			}

			conn_data->maxdiff[i] = diff;
			conn_data->balance_sent = 0;
		} else if(diff > conn_data->maxdiff[i]) {
			conn_data->maxdiff[i] = diff;
		}

		if(conn_data->miner && conn_data->miner->task_time < task_time) {
			conn_data->miner->task_time = task_time;

			if(conn_data->miner->maxdiff[i] > 0) {
				conn_data->miner->prev_diff += conn_data->miner->maxdiff[i];
				conn_data->miner->prev_diff_count++;
			}

			conn_data->miner->maxdiff[i] = diff;
		} else if(diff > conn_data->miner->maxdiff[i]) {
			conn_data->miner->maxdiff[i] = diff;
		}
	}
}

static void register_new_miner(connection_list_element *connection, int index)
{
	miner_list_element *elt;
	struct connection_pool_data *conn_data = &connection->connection_data;

	int exists = 0;
	LL_FOREACH(g_miner_list_head, elt)
	{
		if(memcmp(elt->miner_data.id.data, conn_data->data, sizeof(xdag_hashlow_t)) == 0) {
			conn_data->miner = &elt->miner_data;
			++conn_data->miner->connections_count;
			conn_data->miner->state = MINER_ACTIVE;
			conn_data->state = ACTIVE_CONNECTION;
			exists = 1;
			break;
		}
	}

	if(!exists) {
		struct miner_list_element *new_miner = (struct miner_list_element*)malloc(sizeof(miner_list_element));
		memset(new_miner, 0, sizeof(miner_list_element));
		memcpy(new_miner->miner_data.id.data, conn_data->data, sizeof(struct xdag_field));
		new_miner->miner_data.connections_count = 1;
		new_miner->miner_data.state = MINER_ACTIVE;
		LL_APPEND(g_miner_list_head, new_miner);
		conn_data->miner = &new_miner->miner_data;
		conn_data->state = ACTIVE_CONNECTION;
	}
}

static int recieve_data_from_connection(connection_list_element *connection, int index)
{
	struct connection_pool_data *conn_data = &connection->connection_data;
	int data_size = sizeof(struct xdag_field) - conn_data->data_size;
	data_size = read(conn_data->connection_descriptor.fd, (uint8_t*)conn_data->data + conn_data->data_size, data_size);

	if(data_size <= 0) {
		close_connection(connection, index, "read error");
		return 0;
	}

	conn_data->data_size += data_size;

	if(conn_data->data_size == sizeof(struct xdag_field)) {
		conn_data->data_size = 0;
		dfslib_uncrypt_array(g_crypt, conn_data->data, DATA_SIZE, conn_data->nfield_in++);

		if(!conn_data->block_size && conn_data->data[0] == HEADER_WORD) {
			conn_data->block = malloc(sizeof(struct xdag_block));

			if(!conn_data->block) return 0;

			memcpy(conn_data->block->field, conn_data->data, sizeof(struct xdag_field));
			conn_data->block_size++;
		} else if(conn_data->nfield_in == 1) {
			close_connection(connection, index, "protocol mismatch");
			return 0;
		} else if(conn_data->block_size) {
			memcpy(conn_data->block->field + conn_data->block_size, conn_data->data, sizeof(struct xdag_field));
			conn_data->block_size++;
			if(conn_data->block_size == XDAG_BLOCK_FIELDS) {
				uint32_t crc = conn_data->block->field[0].transport_header >> 32;

				conn_data->block->field[0].transport_header &= (uint64_t)0xffffffffu;

				if(crc == crc_of_array((uint8_t*)conn_data->block, sizeof(struct xdag_block))) {
					conn_data->block->field[0].transport_header = 0;

					pthread_mutex_lock(&g_pool_mutex);

					if(!g_firstb) {
						g_firstb = g_lastb = conn_data->block;
					} else {
						g_lastb->field[0].transport_header = (uintptr_t)conn_data->block;
						g_lastb = conn_data->block;
					}

					pthread_mutex_unlock(&g_pool_mutex);
				} else {
					free(conn_data->block);
				}

				conn_data->block = 0;
				conn_data->block_size = 0;
			}
		} else {
			//share is received
			uint64_t task_index = g_xdag_pool_task_index;
			struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];

			//if (++m->shares_count > SHARES_PER_TASK_LIMIT) {   //if shares count limit is exceded it is considered as spamming and current connection is disconnected
			//	mess = "Spamming of shares";
			//	goto disconnect;	//TODO: get rid of gotos
			//}

			if(conn_data->state == UNKNOWN_ADDRESS) {
				register_new_miner(connection, index);
			} else {
				if(!conn_data->miner) {
					close_connection(connection, index, "Miner is unregistered");
					return 0;
				}
				if(memcmp(conn_data->miner->id.data, conn_data->data, sizeof(xdag_hashlow_t)) != 0) {
					close_connection(connection, index, "Wallet address was unexpectedly changed");
					return 0;
				}
				memcpy(conn_data->miner->id.data, conn_data->data, sizeof(struct xdag_field));	//TODO:do I need to copy whole field?
			}

			xdag_hash_t hash;
			xdag_hash_final(task->ctx0, conn_data->data, sizeof(struct xdag_field), hash);
			xdag_set_min_share(task, conn_data->miner->id.data, hash);
			calculate_nopaid_shares(conn_data, task, hash);
		}
	}

	return 1;
}

static int send_data_to_connection(connection_list_element *connection, int index, int *processed)
{
	struct xdag_field data[2];
	int fields_count = 0;
	struct connection_pool_data *conn_data = &connection->connection_data;

	uint64_t task_index = g_xdag_pool_task_index;
	struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];

	if(conn_data->task_index < task_index) {
		conn_data->task_index = task_index;
		//m->shares_count = 0;
		fields_count = 2;
		memcpy(data, task->task, fields_count * sizeof(struct xdag_field));
	} else if(!conn_data->balance_sent && conn_data->miner && time(0) >= (conn_data->task_time << 6) + 4) {
		conn_data->balance_sent = 1;
		memcpy(data[0].data, conn_data->miner->id.data, sizeof(xdag_hash_t));
		data[0].amount = xdag_get_balance(data[0].data);
		fields_count = 1;
	}

	if(fields_count) {
		*processed = 1;
		for(int j = 0; j < fields_count; ++j) {
			dfslib_encrypt_array(g_crypt, (uint32_t*)(data + j), DATA_SIZE, conn_data->nfield_out++);
		}

		int length = write(conn_data->connection_descriptor.fd, (void*)data, fields_count * sizeof(struct xdag_field));

		if(length != fields_count * sizeof(struct xdag_field)) {
			close_connection(connection, index, "write error");
			return 0;
		}
	}

	return 1;
}

void *pool_main_thread(void *arg)
{
	connection_list_element *elt, *eltmp;

	while(!g_xdag_sync_on) {
		sleep(1);
	}

	for(;;) {
		pthread_mutex_lock(&g_descriptors_mutex);
		const int miners_count = g_connections_count;
		int res = poll(g_fds, miners_count, 1000);
		pthread_mutex_unlock(&g_descriptors_mutex);

		if(!res) continue;

		int index = 0;
		int processed = 0;
		LL_FOREACH_SAFE(g_connection_list_head, elt, eltmp)
		{
			struct pollfd *p = g_fds + index++;

			if(p->revents & POLLNVAL) continue;

			if(p->revents & POLLHUP) {
				processed = 1;
				close_connection(elt, index, "socket hangup");
				continue;
			}

			if(p->revents & POLLERR) {
				processed = 1;
				close_connection(elt, index, "socket error");
				continue;
			}

			if(p->revents & POLLIN) {
				processed = 1;
				if(!recieve_data_from_connection(elt, index)) {
					continue;
				}
			}

			if(p->revents & POLLOUT) {
				if(!send_data_to_connection(elt, index, &processed)) {
					continue;
				}
			}
		}

		if(!processed) {
			sleep(1);
		}
	}

	return 0;
}

void *pool_block_thread(void *arg)
{
	xdag_time_t t0 = 0;
	struct xdag_block *b;
	int res;

	while(!g_xdag_sync_on) {
		sleep(1);
	}

	for(;;) {
		int done = 0;
		uint64_t ntask = g_xdag_pool_task_index;
		struct xdag_pool_task *task = &g_xdag_pool_task[ntask & 1];
		xdag_time_t t = task->task_time;

		if(t > t0) {
			uint64_t *h = g_xdag_mined_hashes[(t - CONFIRMATIONS_COUNT + 1) & (CONFIRMATIONS_COUNT - 1)];

			done = 1;
			t0 = t;

			res = pay_miners(t - CONFIRMATIONS_COUNT + 1);

			xdag_info("%s: %016llx%016llx%016llx%016llx t=%llx res=%d", (res ? "Nopaid" : "Paid  "),
				h[3], h[2], h[1], h[0], (t - CONFIRMATIONS_COUNT + 1) << 16 | 0xffff, res);
		}

		pthread_mutex_lock(&g_pool_mutex);

		if(g_firstb) {
			b = g_firstb;
			g_firstb = (struct xdag_block *)(uintptr_t)b->field[0].transport_header;
			if(!g_firstb) g_lastb = 0;
		} else {
			b = 0;
		}

		pthread_mutex_unlock(&g_pool_mutex);

		if(b) {
			done = 1;
			b->field[0].transport_header = 2;

			res = xdag_add_block(b);
			if(res > 0) {
				xdag_send_new_block(b);
			}
		}

		if(!done) sleep(1);
	}

	return 0;
}

#define diff2pay(d, n) ((n) ? exp((d) / (n) - 20) * (n) : 0)

static double countpay(struct miner_pool_data *miner, int confirmation_index, double *pay)
{
	double sum = 0;
	int diff_count = 0;

	if(miner->maxdiff[confirmation_index] > 0) {
		sum += miner->maxdiff[confirmation_index];
		miner->maxdiff[confirmation_index] = 0;
		diff_count++;
	}

	*pay = diff2pay(sum, diff_count);
	sum += miner->prev_diff;
	diff_count += miner->prev_diff_count;
	miner->prev_diff = 0;
	miner->prev_diff_count = 0;

	return diff2pay(sum, diff_count);
}

static int precalculate_payments(uint64_t *hash, int confirmation_index, struct payment_data *data, double *diff, double *prev_diff, uint64_t *nonce)
{
	miner_list_element *elt;

	data->reward = (xdag_amount_t)(data->balance * g_pool_reward);
	data->pay -= data->reward;

	if(g_pool_fund) {
		if(g_fund_miner.state == MINER_UNKNOWN) {
			xdag_time_t t;
			if(!xdag_address2hash(FUND_ADDRESS, g_fund_miner.id.hash) && xdag_get_block_pos(g_fund_miner.id.hash, &t) >= 0) {
				g_fund_miner.state = MINER_SERVICE;
			}
		}

		if(g_fund_miner.state != MINER_UNKNOWN) {
			data->fund = data->balance * g_pool_fund;
			data->pay -= data->fund;
		}
	}

	data->prev_sum = countpay(&g_pool_miner, confirmation_index, &data->sum);

	int index = 0;
	LL_FOREACH(g_miner_list_head, elt)
	{
		struct miner_pool_data *miner = &elt->miner_data;

		prev_diff[index] = countpay(miner, confirmation_index, &diff[index]);
		data->sum += diff[index];
		data->prev_sum += prev_diff[index];

		if(data->reward_index < 0 && !memcmp(nonce, miner->id.data, sizeof(xdag_hashlow_t))) {
			data->reward_index = index;
		}
		++index;
	}

	if(data->sum > 0) {
		data->direct = data->balance * g_pool_direct;
		data->pay -= data->direct;
	}

	return data->prev_sum;
}

static void transfer_payment(struct miner_pool_data *miner, xdag_amount_t payment_sum, struct xdag_field *fields, int fields_count, int *field_index)
{
	if(!payment_sum) return;

	memcpy(fields[*field_index].data, miner->id.data, sizeof(xdag_hashlow_t));
	fields[*field_index].amount = payment_sum;
	fields[0].amount += payment_sum;

	xdag_log_xfer(fields[0].data, fields[*field_index].data, payment_sum);

	if(++*field_index == fields_count) {
		xdag_create_block(fields, 1, *field_index - 1, 0, 0, NULL);
		*field_index = 1;
		fields[0].amount = 0;
	}
}

static void do_payments(uint64_t *hash, int fields_count, struct payment_data *data, double *diff, double *prev_diff)
{
	miner_list_element *elt;
	struct xdag_field fields[12];
	xdag_amount_t payment_sum;

	memcpy(fields[0].data, hash, sizeof(xdag_hashlow_t));
	fields[0].amount = 0;
	int field_index = 1;

	int index = 0;
	LL_FOREACH(g_miner_list_head, elt)
	{
		struct miner_pool_data *miner = &elt->miner_data;

		payment_sum = data->pay * (prev_diff[index] / data->prev_sum);

		if(data->sum > 0) {
			payment_sum += data->direct * (diff[index] / data->sum);
		}

		if(index == data->reward_index) {
			payment_sum += data->reward;
		}

		transfer_payment(miner, payment_sum, fields, fields_count, &field_index);
	}

	if(g_fund_miner.state != MINER_UNKNOWN) {
		transfer_payment(&g_fund_miner, data->fund, fields, fields_count, &field_index);
	}

	if(field_index > 1) {
		xdag_create_block(fields, 1, field_index - 1, 0, 0, NULL);
	}
}

int pay_miners(xdag_time_t time)
{
	int64_t pos;
	int key, defkey, fields_count;
	double *diff, *prev_diff;
	struct payment_data data;
	miner_list_element *elt;

	memset(&data, 0, sizeof(struct payment_data));
	data.reward_index = -1;

	int miners_count;
	LL_COUNT(g_miner_list_head, elt, miners_count);
	if(!miners_count) return -1;

	int confirmation_index = time & (CONFIRMATIONS_COUNT - 1);
	uint64_t *hash = g_xdag_mined_hashes[confirmation_index];
	uint64_t *nonce = g_xdag_mined_nonce[confirmation_index];

	data.balance = xdag_get_balance(hash);
	if(!data.balance) return -2;

	data.pay = data.balance - (xdag_amount_t)(g_pool_fee * data.balance);
	if(!data.pay) return -3;

	key = xdag_get_key(hash);
	if(key < 0) return -4;

	if(!xdag_wallet_default_key(&defkey)) return -5;

	fields_count = (key == defkey ? 12 : 10);

	pos = xdag_get_block_pos(hash, &time);
	if(pos < 0) return -6;

	struct xdag_block buf;
	struct xdag_block *block = xdag_storage_load(hash, time, pos, &buf);
	if(!block) return -7;

	diff = malloc(2 * miners_count * sizeof(double));
	if(!diff) return -8;
	prev_diff = diff + miners_count;

	if(!precalculate_payments(hash, confirmation_index, &data, diff, prev_diff, nonce)) {
		free(diff);
		return -9;
	}

	do_payments(hash, fields_count, &data, diff, prev_diff);

	free(diff);

	return 0;
}

static const char* miner_state_to_string(int miner_state)
{
	switch(miner_state) {
		case MINER_ACTIVE:
			return "active";
		case MINER_ARCHIVE:
			return "archive";
		default:
			return "unknown";
	}
}

static int print_miner(FILE *out, int index, struct miner_pool_data *miner)
{
	double sum = miner->prev_diff;
	int count = miner->prev_diff_count;
	//char buf[32], buf2[64];
	//uint32_t ip = miner->ip;

	for(int j = 0; j < CONFIRMATIONS_COUNT; ++j) {
		if(miner->maxdiff[j] > 0) {
			sum += miner->maxdiff[j];
			count++;
		}
	}

	//TODO: fix printing of connections data
	//sprintf(buf, "%u.%u.%u.%u:%u", ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(miner->port));
	//sprintf(buf2, "%llu/%llu", (unsigned long long)miner->nfield_in * sizeof(struct xdag_field),
	//	(unsigned long long)m->nfield_out * sizeof(struct xdag_field));

	//fprintf(out, "%3d. %s  %s  %-21s  %-16s  %lf\n", n, xdag_hash2address(m->id.data),
	//	(m->state & MINER_FREE ? "free   " : (m->state & MINER_ARCHIVE ? "archive" :
	//	(m->state & MINER_ADDRESS ? "active " : "badaddr"))), buf, buf2, diff2pay(sum, count));

	fprintf(out, "%3d. %s  %s  %lf\n", index, xdag_hash2address(miner->id.data),
		miner_state_to_string(miner->state), diff2pay(sum, count));

	return miner->state == MINER_ACTIVE ? 1 : 0;
}

/* output to the file a list of miners */
int xdag_print_miners(FILE *out)
{
	fprintf(out, "List of miners:\n"
		" NN  Address for payment to            Status   IP and port            in/out bytes      nopaid shares\n"
		"------------------------------------------------------------------------------------------------------\n");
	int count_active = print_miner(out, -1, &g_pool_miner);

	miner_list_element *elt;
	int index = 0;
	LL_FOREACH(g_miner_list_head, elt)
	{
		struct miner_pool_data *miner = &elt->miner_data;
		count_active += print_miner(out, index++, miner);
	}

	fprintf(out,
		"------------------------------------------------------------------------------------------------------\n"
		"Total %d active miners.\n", count_active);

	return count_active;
}
