// pool logic

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "block.h"
#include "sync.h"
#include "mining_common.h"
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "uthash\utlist.h"

#define MAX_MINERS_COUNT               4096
#define XDAG_POOL_CONFIRMATIONS_COUNT  16
#define DATA_SIZE                      (sizeof(struct xdag_field) / sizeof(uint32_t))
#define CONFIRMATIONS_COUNT            XDAG_POOL_CONFIRMATIONS_COUNT   /* 16 */

#define START_MINERS_COUNT     256
#define START_MINERS_IP_COUNT  8
#define HEADER_WORD            0x3fca9e2bu
#define FUND_ADDRESS           "FQglVQtb60vQv2DOWEUL7yh3smtj7g1s"  /* community fund */
#define SHARES_PER_TASK_LIMIT  20                                  /* maximum count of shares per task */

enum miner_state {
	MINER_ACTIVE = 1,
	MINER_ARCHIVE = 2
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

xdag_hash_t g_xdag_mined_hashes[CONFIRMATIONS_COUNT], g_xdag_mined_nonce[CONFIRMATIONS_COUNT];

static int g_max_miners_count = START_MINERS_COUNT, g_max_miner_ip_count = START_MINERS_IP_COUNT;
static int g_miners_count = 0;
static double g_pool_fee = 0, g_pool_reward = 0, g_pool_direct = 0, g_pool_fund = 0;
static struct xdag_block *g_firstb = 0, *g_lastb = 0;

connection_list_element *g_connection_list_head = NULL;
connection_list_element *g_miner_list_head = NULL;
pthread_mutex_t g_descriptors_mutex = PTHREAD_MUTEX_INITIALIZER;

int open_pool_connection(char *pool_arg)
{
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	int rcvbufsize = 1024;
	int reuseaddr = 1;
	const char *message;
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

int connection_can_be_accepted(int sock, struct sockaddr_in *peeraddr)
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
	LL_FOREACH(g_connection_list_head, elt) {
		if(elt->connection_data.ip == peeraddr->sin_addr.s_addr) {
			if(++count >= g_max_miner_ip_count) {
				return 0;
			}
		}
	}

	return 1;
}

void rebuild_descriptors_array() {
	connection_list_element *elt;
	int index = 0;	
	LL_FOREACH(g_connection_list_head, elt)	{
		memcpy(g_fds + index, &elt->connection_data.connection_descriptor, sizeof(struct pollfd));
		++index;
	}
}

void *pool_net_thread(void *arg)
{
	const char *pool_arg = (const char*)arg;
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	struct miner *m;
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
			return 0;
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
		++g_miners_count;
		rebuild_descriptors_array();
		pthread_mutex_unlock(&g_descriptors_mutex);

		xdag_info("Pool  : miner %d connected from %u.%u.%u.%u:%u", g_miners_count,
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(new_connection->connection_data.port));
	}

	return 0;
}

void close_connection(connection_list_element *connection, int index, const char *message)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

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

	LL_DELETE(g_connection_list_head, connection);
	free(connection);

	xdag_info("Pool: miner %d disconnected from %u.%u.%u.%u:%u by %s", index,
		ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(port), message);
}

static void calculate_nopaid_shares(connection_list_element *connection, struct xdag_pool_task *task, xdag_hash_t hash)
{
	struct connection_pool_data *conn_data = &connection->connection_data;
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

		if(!conn_data->miner && conn_data->miner->task_time < task_time) {
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

int register_new_miner(connection_list_element *connection, int index)
{
	miner_list_element *elt;
	struct connection_pool_data *conn_data = &connection->connection_data;

	int exists = 0;
	LL_FOREACH(g_miner_list_head, elt) {
		if(memcmp(elt->miner_data.id.data, conn_data->data, sizeof(xdag_hashlow_t))) {
			conn_data->miner = &elt->miner_data;
			++conn_data->miner->connections_count;
			conn_data->miner->state = MINER_ACTIVE;
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
		conn_data->miner = &elt->miner_data;
		LL_APPEND(g_miner_list_head, new_miner);
	}
}

int recieve_data_from_connection(connection_list_element *connection, int index)
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

			if(!conn_data->block) return 1;

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
				}
				if(!memcmp(conn_data->miner->id.data, conn_data->data, sizeof(xdag_hashlow_t))) {
					close_connection(connection, index, "Wallet address was unexpectedly changed");
				}
				memcpy(conn_data->miner->id.data, conn_data->data, sizeof(struct xdag_field));	//TODO:do I need to copy whole field?
			}

			xdag_hash_t hash;
			xdag_hash_final(task->ctx0, conn_data->data, sizeof(struct xdag_field), hash);
			xdag_set_min_share(task, conn_data->miner->id.data, hash);
			calculate_nopaid_shares(conn_data, task, hash);
		}
	}
}

int send_data_to_connection(connection_list_element *connection, int index)
{
	struct xdag_field data[2];
	int nfld = 0;
	struct connection_pool_data *conn_data = &connection->connection_data;

	uint64_t task_index = g_xdag_pool_task_index;
	struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];

	if(conn_data->task_index < task_index) {
		conn_data->task_index = task_index;
		//m->shares_count = 0;
		nfld = 2;
		memcpy(data, task->task, nfld * sizeof(struct xdag_field));
	} else if(!conn_data->balance_sent && !conn_data->miner && time(0) >= (conn_data->task_time << 6) + 4) {
		conn_data->balance_sent = 1;
		memcpy(data[0].data, conn_data->miner->id.data, sizeof(xdag_hash_t));
		data[0].amount = xdag_get_balance(data[0].data);
		nfld = 1;
	}

	if(nfld) {
		for(int j = 0; j < nfld; ++j) {
			dfslib_encrypt_array(g_crypt, (uint32_t*)(data + j), DATA_SIZE, conn_data->nfield_out++);
		}

		int length = write(conn_data->connection_descriptor.fd, (void*)data, nfld * sizeof(struct xdag_field));

		if(length != nfld * sizeof(struct xdag_field)) {
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
		const int miners_count = g_miners_count;
		int res = poll(g_fds, miners_count, 1000);
		pthread_mutex_unlock(&g_descriptors_mutex);

		if(!res) continue;
		
		int index = 0;
		LL_FOREACH_SAFE(g_connection_list_head, elt, eltmp) {
			struct pollfd *p = g_fds + index++;

			if(p->revents & POLLNVAL) continue;

			if(p->revents & POLLHUP) {
				close_connection(elt, index, "socket hangup");
				continue;
			}

			if(p->revents & POLLERR) {
				close_connection(elt, index, "socket error");
				continue;
			}

			if(p->revents & POLLIN) {
				if(!recieve_data_from_connection(elt, index)) {
					continue;
				}
			}

			if(p->revents & POLLOUT) {
				if(!send_data_to_connection(elt, index)) {
					continue;
				}
			}
		}

		sleep(1);
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

static double countpay(struct miner *m, int i, double *pay)
{
	double sum = 0;
	int n = 0;

	if(m->maxdiff[i] > 0) {
		sum += m->maxdiff[i]; m->maxdiff[i] = 0; n++;
	}

	*pay = diff2pay(sum, n);
	sum += m->prev_diff;
	n += m->prev_diff_count;
	m->prev_diff = 0;
	m->prev_diff_count = 0;

	return diff2pay(sum, n);
}

static int pay_miners(xdag_time_t time)
{
	struct xdag_field fields[12];
	struct xdag_block buf, *b;
	xdag_amount_t direct = 0, fund = 0;
	struct miner *m;
	int64_t pos;
	int i, reward_ind = -1, key, defkey, nfields, nfld;
	double *diff, *prev_diff, sum, prev_sum, topay;

	int miners_count = g_miners_count;

	if(!miners_count) return -1;

	int n = time & (CONFIRMATIONS_COUNT - 1);
	uint64_t *h = g_xdag_mined_hashes[n];
	uint64_t *nonce = g_xdag_mined_nonce[n];
	xdag_amount_t balance = xdag_get_balance(h);

	if(!balance) return -2;

	xdag_amount_t pay = balance - (xdag_amount_t)(g_pool_fee * balance);
	if(!pay) return -3;

	xdag_amount_t reward = (xdag_amount_t)(balance * g_pool_reward);
	pay -= reward;

	if(g_pool_fund) {
		if(!(g_fund_miner.state & MINER_ADDRESS)) {
			xdag_time_t t;
			if(!xdag_address2hash(FUND_ADDRESS, g_fund_miner.id.hash) && xdag_get_block_pos(g_fund_miner.id.hash, &t) >= 0) {
				g_fund_miner.state |= MINER_ADDRESS;
			}
		}

		if(g_fund_miner.state & MINER_ADDRESS) {
			fund = balance * g_pool_fund;
			pay -= fund;
		}
	}

	key = xdag_get_key(h);
	if(key < 0) return -4;

	if(!xdag_wallet_default_key(&defkey)) return -5;

	nfields = (key == defkey ? 12 : 10);

	pos = xdag_get_block_pos(h, &time);
	if(pos < 0) return -6;

	b = xdag_storage_load(h, time, pos, &buf);
	if(!b) return -7;

	diff = malloc(2 * miners_count * sizeof(double));
	if(!diff) return -8;

	prev_diff = diff + miners_count;
	prev_sum = countpay(&g_local_miner, n, &sum);

	for(i = 0; i < miners_count; ++i) {
		m = g_miners + i;

		if(m->state & MINER_FREE || !(m->state & MINER_ADDRESS)) continue;

		prev_diff[i] = countpay(m, n, &diff[i]);
		sum += diff[i];
		prev_sum += prev_diff[i];

		if(reward_ind < 0 && !memcmp(nonce, m->id.data, sizeof(xdag_hashlow_t))) {
			reward_ind = i;
		}
	}

	if(!prev_sum) return -9;

	if(sum > 0) {
		direct = balance * g_pool_direct;
		pay -= direct;
	}

	memcpy(fields[0].data, h, sizeof(xdag_hashlow_t));
	fields[0].amount = 0;

	for(nfld = 1, i = 0; i <= miners_count; ++i) {
		if(i < miners_count) {
			m = g_miners + i;

			if(m->state & MINER_FREE || !(m->state & MINER_ADDRESS)) continue;

			topay = pay * (prev_diff[i] / prev_sum);

			if(sum > 0) {
				topay += direct * (diff[i] / sum);
			}

			if(i == reward_ind) {
				topay += reward;
			}
		} else {
			m = &g_fund_miner;
			if(!(m->state & MINER_ADDRESS)) continue;
			topay = fund;
		}

		if(!topay) continue;

		memcpy(fields[nfld].data, m->id.data, sizeof(xdag_hashlow_t));
		fields[nfld].amount = topay;
		fields[0].amount += topay;

		xdag_log_xfer(fields[0].data, fields[nfld].data, topay);

		if(++nfld == nfields) {
			xdag_create_block(fields, 1, nfld - 1, 0, 0, NULL);
			nfld = 1;
			fields[0].amount = 0;
		}
	}

	if(nfld > 1) {
		xdag_create_block(fields, 1, nfld - 1, 0, 0, NULL);
	}

	return 0;
}