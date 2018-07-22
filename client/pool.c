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
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#endif
#include "block.h"
#include "sync.h"
#include "mining_common.h"
#include "pool.h"
#include "address.h"
#include "utils/moving_statistics/moving_average.h"
#include "commands.h"
#include "storage.h"
#include "transport.h"
#include "wallet.h"
#include "system.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "uthash/utlist.h"
#include "uthash/uthash.h"

//TODO: why do we need these two definitions?
#define START_MINERS_COUNT     256
#define START_MINERS_IP_COUNT  8

#define FUND_ADDRESS                       "FQglVQtb60vQv2DOWEUL7yh3smtj7g1s" /* community fund */
#define SHARES_PER_TASK_LIMIT              20                                 /* maximum count of shares per task */
#define DEFAUL_CONNECTIONS_PER_MINER_LIMIT 100
#define WORKERNAME_HEADER_WORD             0xf46b9853u

struct nonce_hash {
	uint64_t key;
	UT_hash_handle hh;
};

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
	uint64_t task_index;
	struct nonce_hash *nonces;
	xdag_hash_t last_min_hash;
	long double mean_log_difficulty;
	uint32_t bounded_task_counter;
	time_t registered_time;
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
	time_t balance_refreshed_time;
	uint32_t shares_count;
	time_t last_share_time;
	int deleted;
	const char* disconnection_reason;
	xdag_hash_t last_min_hash;
	long double mean_log_difficulty;
	uint32_t bounded_task_counter;
	char* worker_name;
	time_t connected_time;
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

static uint32_t g_max_connections_count = START_MINERS_COUNT, g_max_miner_ip_count = START_MINERS_IP_COUNT;
static uint32_t g_connections_per_miner_limit = DEFAUL_CONNECTIONS_PER_MINER_LIMIT;
static uint32_t g_connections_count = 0;
static double g_pool_fee = 0, g_pool_reward = 0, g_pool_direct = 0, g_pool_fund = 0;
static struct xdag_block *g_firstb = 0, *g_lastb = 0;

static int g_stop_general_mining = 1;

static struct miner_pool_data g_pool_miner;
static struct miner_pool_data g_fund_miner;
static struct pollfd *g_fds;

static connection_list_element *g_connection_list_head = NULL;
static connection_list_element *g_accept_connection_list_head = NULL;
static miner_list_element *g_miner_list_head = NULL;
static uint32_t g_connection_changed = 0;
static pthread_mutex_t g_connections_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

int pay_miners(xdag_time_t time);
void remove_inactive_miners(void);

void *general_mining_thread(void *arg);
void *pool_net_thread(void *arg);
void *pool_main_thread(void *arg);
void *pool_block_thread(void *arg);
void *pool_remove_inactive_connections(void *arg);

void update_mean_log_diff(struct connection_pool_data *, struct xdag_pool_task *, xdag_hash_t);

/* initialization of the pool */
int xdag_initialize_pool(const char *pool_arg)
{
	pthread_t th;

	memset(&g_pool_miner, 0, sizeof(struct miner_pool_data));
	memset(&g_fund_miner, 0, sizeof(struct miner_pool_data));

	xdag_get_our_block(g_pool_miner.id.data);
	g_pool_miner.state = MINER_SERVICE;

	g_fds = malloc(MAX_CONNECTIONS_COUNT * sizeof(struct pollfd));
	if(!g_fds) return -1;

	int err = pthread_create(&th, 0, pool_net_thread, (void*)pool_arg);
	if(err != 0) {
		printf("create pool_net_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("detach pool_net_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_create(&th, 0, pool_main_thread, 0);
	if(err != 0) {
		printf("create pool_main_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("detach pool_main_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_create(&th, 0, pool_block_thread, 0);
	if(err != 0) {
		printf("create pool_block_thread failed: %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("detach pool_block_thread failed: %s\n", strerror(err));
		return -1;
	}

	err = pthread_create(&th, 0, pool_remove_inactive_connections, 0);
	if(err != 0) {
		printf("create pool_remove_inactive_connections failed: %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("detach pool_remove_inactive_connections failed: %s\n", strerror(err));
		return -1;
	}

	xdag_mess("Starting general mining thread...");

	g_stop_general_mining = 0;

	err = pthread_create(&th, 0, general_mining_thread, 0);
	if(err != 0) {
		printf("create general_mining_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("detach general_mining_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	return 0;
}

void *general_mining_thread(void *arg)
{
	while(!g_xdag_sync_on && !g_stop_general_mining) {
		sleep(1);
	}

	while(!g_stop_general_mining) {
		xdag_create_block(0, 0, 0, 0, xdag_main_time() << 16 | 0xffff, NULL);
	}

	xdag_mess("Stopping general mining thread...");

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
		int max_connection_count_input;
		int open_max = (int)sysconf(_SC_OPEN_MAX);

		sscanf(pool_config, "%d", &max_connection_count_input);

		if(max_connection_count_input < 0) {
			max_connection_count_input = 0;
			xdag_warn("pool: wrong connections count");
		} else if(max_connection_count_input > MAX_CONNECTIONS_COUNT) {
			max_connection_count_input = MAX_CONNECTIONS_COUNT;
			xdag_warn("pool: exceed max connections count %d", MAX_CONNECTIONS_COUNT);
		} else if(max_connection_count_input > open_max - 64) {
			max_connection_count_input = open_max - 64;
			xdag_warn("pool: exceed max open files %d", open_max - 64);
		}
		g_max_connections_count = max_connection_count_input;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%d", &g_max_miner_ip_count);

		if(g_max_miner_ip_count <= 0)
			g_max_miner_ip_count = 1;
	}

	pool_config = strtok_r(0, " \t\r\n:", &lasts);
	if(pool_config) {
		sscanf(pool_config, "%d", &g_connections_per_miner_limit);

		if(g_connections_per_miner_limit <= 0)
			g_connections_per_miner_limit = 1;
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

	sprintf(buf, "%d:%d:%d:%.2lf:%.2lf:%.2lf:%.2lf", g_max_connections_count, g_max_miner_ip_count, g_connections_per_miner_limit,
		g_pool_fee * 100, g_pool_reward * 100, g_pool_direct * 100, g_pool_fund * 100);

	return buf;
}

static int open_pool_connection(const char *pool_arg)
{
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	struct sockaddr_in peeraddr;
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
		xdag_err("pool: cannot bind a socket (error %s)", strerror(res));
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

	//firstly we check that total count of connection did not exceed max count of connection
	if(g_connections_count >= g_max_connections_count) {
		xdag_warn("Max connections %d exceed, new connections are not accepted.", g_max_connections_count);
		return 0;
	}

	//then we check that count of connections with the same IP address did not exceed the limit
	uint32_t count = 0;
	LL_FOREACH(g_connection_list_head, elt)
	{
		if(elt->connection_data.ip == peeraddr->sin_addr.s_addr) {
			if(++count >= g_max_miner_ip_count) {
				int ip = elt->connection_data.ip;
				xdag_warn("Max connection %d for ip %u.%u.%u.%u:%u exceed, new connections are not accepted.",
					g_max_miner_ip_count, ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff,
					ntohs(elt->connection_data.port));
				return 0;
			}
		}
	}

	LL_FOREACH(g_accept_connection_list_head, elt)
	{
		if(elt->connection_data.ip == peeraddr->sin_addr.s_addr) {
			if(++count >= g_max_miner_ip_count) {
				int ip = elt->connection_data.ip;
				xdag_warn("Max connection %d for ip %u.%u.%u.%u:%u exceed, new connections are not accepted.",
					g_max_miner_ip_count, ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff,
					ntohs(elt->connection_data.port));
				return 0;
			}
		}
	}

	return 1;
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
		xdag_err("Pool: open connection error!");
		return 0;
	}

	// Now, listen for a connection
	int res = listen(sock, MAX_CONNECTIONS_COUNT);    // "1" is the maximal length of the queue
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

		pthread_mutex_lock(&g_connections_mutex);
		if(!connection_can_be_accepted(sock, &peeraddr)) {
			close(fd);
			pthread_mutex_unlock(&g_connections_mutex);
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
		new_connection->connection_data.connected_time = time(0);
		new_connection->connection_data.last_share_time = new_connection->connection_data.connected_time; // we set time of last share to the current time in order to avoid immediate disconnection

		LL_APPEND(g_accept_connection_list_head, new_connection);
		++g_connections_count;
		pthread_mutex_unlock(&g_connections_mutex);

		xdag_info("Pool  : miner %d connected from %u.%u.%u.%u:%u", g_connections_count,
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(new_connection->connection_data.port));
	}

	return 0;
}

static void close_connection(connection_list_element *connection, const char *message)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

	pthread_mutex_lock(&g_connections_mutex);
	LL_DELETE(g_connection_list_head, connection);
	--g_connections_count;
	g_connection_changed = 1;

	close(conn_data->connection_descriptor.fd);

	if(conn_data->block) {
		free(conn_data->block);
	}
	if(conn_data->worker_name) {
		free(conn_data->worker_name);
	}

	if(conn_data->miner) {
		--conn_data->miner->connections_count;
		if(conn_data->miner->connections_count == 0) {
			conn_data->miner->state = MINER_ARCHIVE;
		}
	}
	pthread_mutex_unlock(&g_connections_mutex);

	uint32_t ip = conn_data->ip;
	uint16_t port = conn_data->port;

	if(conn_data->miner) {
		char address_buf[33];
		xdag_hash2address(conn_data->miner->id.data, address_buf);
		xdag_info("Pool: miner %s disconnected from %u.%u.%u.%u:%u by %s", address_buf,
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(port), message);
	} else {
		xdag_info("Pool: disconnected from %u.%u.%u.%u:%u by %s",
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(port), message);
	}

	free(connection);
}


/* @method      :- calculate_nopaid_shares
+  @param       :-
+               struct connection_pool_data* connection data :- miner's side data
+               struct xdag_pool_task* task data             :- pool's side data
+               xdag_hash_t hash                             :- is a digest computed by the pool starting from the
+                                                            miner's data 'conn_data->data' and pool side context task->ctx0
+  @return      :- void
+  @description :- calculate nopaid share */

static void calculate_nopaid_shares(struct connection_pool_data *conn_data, struct xdag_pool_task *task, xdag_hash_t hash)
{
	const xdag_time_t task_time = task->task_time;

	if(conn_data->task_time <= task_time) { // At the beginning conn_data->task_time=0. conn_data->task_time > task_time isn't accepted.
		double diff = ((uint64_t*)hash)[2];
		int i = task_time & (CONFIRMATIONS_COUNT - 1);	// CONFIRMATION_COUNT-1=15d=1111b, thus it just cut task_time to its 4 least significant bit

		// %%%%%% ldexp(double a, int b) -> ldexp(diff, -64) will return [diff/2^64] %%%%%%
		// Since max value of diff is 0xFFFFFFFFFFFFFFFF (it is a 64bit unsigned integer variable)
		// and 2^64 is 0xFFFFFFFFFFFFFFFF, ldexp(diff, -64) will return exactly 1 iff
		// diff value is equal to 0xFFFFFFFFFFFFFFFF (can't be higher by definition).
		// But because of the approximation from double to int
		// even when diff is "around" 0xFFFFFFFFFFFFFFFF diff will be 1.
		// Test: for diff >= FFFFFFFFFFFFFC00 (18446744073709550592) ldexp(diff, -64)=1
		// Test: for diff <= FFFFFFFFFFFFFBFF (18446744073709550591) ldexp(diff, -64)=0
		// Still need to investigate the purpose of using ldexp function to do it.

		// %%%%%% 		diff += ((uint64_t*)hash)[3];			     %%%%%%
		// Given that hash[3] is the most significant part of the 256 bit number
		// hash[3] || hash[2] || hash[1] || hash[0]
		// If, as explained previously, hash[2] is near its possible maximum value
		// then diff will be equal to hash[3]+1.

		// %%%%%% 		           diff 			     %%%%%%
		// At this point, diff, seems to be a condensate approximated representation 
		// of the 256 bit number hash[3] || hash[2] || hash[1] || hash[0].

		diff = ldexp(diff, -64);
		diff += ((uint64_t*)hash)[3];

		if(diff < 1) diff = 1;
		diff = 46 - log(diff);

		// Adding share for connection
		if(conn_data->task_time < task_time) { // conn_data->task_time will keep old value until pool doesn't accept the share of the task.
			conn_data->task_time = task_time;  // this will prevent to count more share for the same task, cannot join this block a new time for same task.

			if(conn_data->maxdiff[i] > 0) {
				conn_data->prev_diff += conn_data->maxdiff[i];
				conn_data->prev_diff_count++;
			}

			conn_data->maxdiff[i] = diff;
			// share already counted, but we will update the maxdiff so the most difficult share will be counted.
		} else if(diff > conn_data->maxdiff[i]) {
			conn_data->maxdiff[i] = diff;
		}

		// Adding share for miner
		if(conn_data->miner->task_time < task_time) {
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

static int register_new_miner(connection_list_element *connection)
{
	miner_list_element *elt;
	struct connection_pool_data *conn_data = &connection->connection_data;

	xdag_time_t tm;
	const int64_t position = xdag_get_block_pos((const uint64_t*)conn_data->data, &tm);
	if(position < 0) {
		char address_buf[33];
		char message[100];
		xdag_hash2address((const uint64_t*)conn_data->data, address_buf);
		sprintf(message, "Miner's address is unknown (%s)", address_buf);
		close_connection(connection, message);
		return 0;
	}

	int exists = 0;
	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH(g_miner_list_head, elt)
	{
		if(memcmp(elt->miner_data.id.data, conn_data->data, sizeof(xdag_hashlow_t)) == 0) {
			if(elt->miner_data.connections_count >= g_connections_per_miner_limit) {
				pthread_mutex_unlock(&g_connections_mutex);
				close_connection(connection, "Max count of connections per miner is exceeded");
				return 0;
			}

			conn_data->miner = &elt->miner_data;
			++conn_data->miner->connections_count;
			conn_data->miner->state = MINER_ACTIVE;
			conn_data->state = ACTIVE_CONNECTION;
			exists = 1;
			break;
		}
	}
	pthread_mutex_unlock(&g_connections_mutex);

	if(!exists) {
		pthread_mutex_lock(&g_connections_mutex);
		struct miner_list_element *new_miner = (struct miner_list_element*)malloc(sizeof(miner_list_element));
		memset(new_miner, 0, sizeof(miner_list_element));
		memcpy(new_miner->miner_data.id.data, conn_data->data, sizeof(struct xdag_field));
		new_miner->miner_data.connections_count = 1;
		new_miner->miner_data.state = MINER_ACTIVE;
		new_miner->miner_data.registered_time = time(0);
		LL_APPEND(g_miner_list_head, new_miner);
		conn_data->miner = &new_miner->miner_data;
		conn_data->state = ACTIVE_CONNECTION;
		pthread_mutex_unlock(&g_connections_mutex);
	}

	return 1;
}

static void clear_nonces_hashtable(struct miner_pool_data *miner)
{
	struct nonce_hash *eln, *tmp;
	HASH_ITER(hh, miner->nonces, eln, tmp)
	{
		HASH_DEL(miner->nonces, eln);
		free(eln);
	}
}

static int share_can_be_accepted(struct miner_pool_data *miner, xdag_hash_t share, uint64_t task_index)
{
	if(!miner) {
		xdag_err("conn_data->miner is null");
		return 0;
	}
	struct nonce_hash *eln;
	uint64_t nonce = share[3];
	if(miner->task_index != task_index) {
		clear_nonces_hashtable(miner);
		miner->task_index = task_index;
	} else {
		HASH_FIND(hh, miner->nonces, &nonce, sizeof(uint64_t), eln);
		if(eln != NULL) {
			return 0;	// we received the same nonce and will ignore duplicate
		}
	}
	eln = (struct nonce_hash*)malloc(sizeof(struct nonce_hash));
	eln->key = nonce;
	HASH_ADD(hh, miner->nonces, key, sizeof(uint64_t), eln);
	return 1;
}

// checks if received data belongs to block and processes that block
// returns:
// -1 - error
// 0 - received data does not belong to block
// 1 - block data is processed
static int is_block_data_received(connection_list_element *connection)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

	if(!conn_data->block_size && conn_data->data[0] == BLOCK_HEADER_WORD) {
		conn_data->block = malloc(sizeof(struct xdag_block));

		if(!conn_data->block) {
			return -1;
		}

		memcpy(conn_data->block->field, conn_data->data, sizeof(struct xdag_field));
		conn_data->block_size++;
	} else if(conn_data->nfield_in == 1) {
		close_connection(connection, "protocol mismatch");
		return -1;
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
		return 0;
	}

	return 1;
}

// checks if received data belongs to worker name
// returns:
// 0 - received data does not belong to worker name
// 1 - worker name is processed
static int is_worker_name_received(connection_list_element *connection)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

	if(conn_data->nfield_in == 17 && conn_data->data[0] == WORKERNAME_HEADER_WORD) {
		size_t worker_name_len = strnlen((const char*)&conn_data->data[1], 28);
		if(worker_name_len) {
			conn_data->worker_name = (char*)malloc(worker_name_len + 1);
			memcpy(conn_data->worker_name, (const char*)&conn_data->data[1], worker_name_len);
			conn_data->worker_name[worker_name_len] = 0;
			replace_all_nonprintable_characters(conn_data->worker_name, -1, '_');
			return 1;
		}
	}

	return 0;
}

// processes received share
// returns:
// 0 - error
// 1 - success
static int process_received_share(connection_list_element *connection)
{
	struct connection_pool_data *conn_data = &connection->connection_data;

	const uint64_t task_index = g_xdag_pool_task_index;
	struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];

	if(++conn_data->shares_count > SHARES_PER_TASK_LIMIT) {   //if shares count limit is exceded it is considered as spamming and current connection is disconnected
		close_connection(connection, "Spamming of shares");
		return 0;
	}

	if(conn_data->state == UNKNOWN_ADDRESS) {
		if(!register_new_miner(connection)) {
			return 0;
		}
	} else {
		if(!conn_data->miner) {
			close_connection(connection, "Miner is unregistered");
			return 0;
		}
		if(memcmp(conn_data->miner->id.data, conn_data->data, sizeof(xdag_hashlow_t)) != 0) {
			close_connection(connection, "Wallet address was unexpectedly changed");
			return 0;
		}
		memcpy(conn_data->miner->id.data, conn_data->data, sizeof(struct xdag_field));	//TODO:do I need to copy whole field?
	}

	conn_data->last_share_time = time(0);

	if(share_can_be_accepted(conn_data->miner, (uint64_t*)conn_data->data, task_index)) {
		xdag_hash_t hash;
		xdag_hash_final(task->ctx0, conn_data->data, sizeof(struct xdag_field), hash);
		xdag_set_min_share(task, conn_data->miner->id.data, hash);
		update_mean_log_diff(conn_data, task, hash);
		calculate_nopaid_shares(conn_data, task, hash);
	}

	return 1;
}

static int receive_data_from_connection(connection_list_element *connection)
{
#if _DEBUG
	int ip = connection->connection_data.ip;
	xdag_debug("Pool  : receive data from %u.%u.%u.%u:%u",
		ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(connection->connection_data.port));
#endif

	struct connection_pool_data *conn_data = &connection->connection_data;
	size_t data_size = sizeof(struct xdag_field) - conn_data->data_size;
	data_size = read(conn_data->connection_descriptor.fd, (uint8_t*)conn_data->data + conn_data->data_size, data_size);

	if(data_size <= 0) {
		char message[100];
		sprintf(message, "read error : %s", strerror(errno));
		close_connection(connection, message);
		return 0;
	}

	conn_data->data_size += data_size;

	if(conn_data->data_size == sizeof(struct xdag_field)) { //32
		conn_data->data_size = 0;
		dfslib_uncrypt_array(g_crypt, conn_data->data, DATA_SIZE, conn_data->nfield_in++);

		int result = is_block_data_received(connection);
		if(result < 0) {
			return 0;
		}
		if(result > 0) {
			return 1;
		}

		result = is_worker_name_received(connection);
		if(result > 0) {
			return 1;
		}

		//share is received
		if(!process_received_share(connection)) {
			return 0;
		}
	}

	return 1;
}

static int send_data_to_connection(connection_list_element *connection, int *processed)
{
	struct xdag_field data[2];
	int fields_count = 0;
	struct connection_pool_data *conn_data = &connection->connection_data;

	const uint64_t task_index = g_xdag_pool_task_index;
	struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];
	time_t current_time = time(0);

	if(conn_data->task_index < task_index) {
		conn_data->task_index = task_index;
		conn_data->shares_count = 0;
		fields_count = 2;
		memcpy(data, task->task, fields_count * sizeof(struct xdag_field));
	} else if(conn_data->miner && current_time - conn_data->balance_refreshed_time >= 10) {  //refresh balance each 10 seconds
		//TODO: optimize refreshing of balance
		conn_data->balance_refreshed_time = current_time;
		memcpy(data[0].data, conn_data->miner->id.data, sizeof(xdag_hash_t));
		data[0].amount = xdag_get_balance(data[0].data);
		fields_count = 1;
	}

	if(fields_count) {
		*processed = 1;
		for(int j = 0; j < fields_count; ++j) {
			dfslib_encrypt_array(g_crypt, (uint32_t*)(data + j), DATA_SIZE, conn_data->nfield_out++);
		}

		size_t length = write(conn_data->connection_descriptor.fd, (void*)data, fields_count * sizeof(struct xdag_field));

		if(length != fields_count * sizeof(struct xdag_field)) {
			char message[100];
			sprintf(message, "write error  %s : write %zu bytes of %lu bytes", strerror(errno), length, fields_count * sizeof(struct xdag_field));
			close_connection(connection, message);
			return 0;
		}
	}

	return 1;
}

void *pool_main_thread(void *arg)
{
	while(!g_xdag_sync_on) {
		sleep(1);
	}

	connection_list_element *elt, *eltmp;

	for(;;) {
		pthread_mutex_lock(&g_connections_mutex);

		// move accept connection to g_connection_list_head.
		LL_FOREACH_SAFE(g_accept_connection_list_head, elt, eltmp)
		{
			LL_DELETE(g_accept_connection_list_head, elt);
			LL_APPEND(g_connection_list_head, elt);
			g_connection_changed = 1;
		}
		int index = 0;
		if(g_connection_changed) {
			g_connection_changed = 0;
			LL_FOREACH(g_connection_list_head, elt)
			{
				memcpy(g_fds + index, &elt->connection_data.connection_descriptor, sizeof(struct pollfd));
				++index;
			}
		}

		int connections_count = g_connections_count;
		pthread_mutex_unlock(&g_connections_mutex);

		int res = poll(g_fds, connections_count, 1000);
		if(!res) continue;

		index = 0;
		int processed = 0;
		LL_FOREACH_SAFE(g_connection_list_head, elt, eltmp)
		{
			struct pollfd *p = g_fds + index++;

			if(elt->connection_data.deleted) {
				close_connection(elt, elt->connection_data.disconnection_reason);
				continue;
			}

			if(p->revents & POLLNVAL) {
				continue;
			}

			if(p->revents & POLLHUP) {
				processed = 1;
				close_connection(elt, "socket hangup");
				continue;
			}

			if(p->revents & POLLERR) {
				processed = 1;
				close_connection(elt, "socket error");
				continue;
			}

			if(p->revents & POLLIN) {
				processed = 1;
				if(!receive_data_from_connection(elt)) {
					continue;
				}
			}

			if(p->revents & POLLOUT) {
				if(!send_data_to_connection(elt, &processed)) {
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
	xdag_time_t prev_task_time = 0;
	struct xdag_block *b;
	int res;

	while(!g_xdag_sync_on) {
		sleep(1);
	}

	for(;;) {
		int processed = 0;
		const uint64_t task_index = g_xdag_pool_task_index;
		struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];
		const xdag_time_t current_task_time = task->task_time;

		if(current_task_time > prev_task_time) {
			uint64_t *hash = g_xdag_mined_hashes[(current_task_time - CONFIRMATIONS_COUNT + 1) & (CONFIRMATIONS_COUNT - 1)];

			processed = 1;
			prev_task_time = current_task_time;

			res = pay_miners(current_task_time - CONFIRMATIONS_COUNT + 1);
			remove_inactive_miners();

			xdag_info("%s: %016llx%016llx%016llx%016llx t=%llx res=%d", (res ? "Nopaid" : "Paid  "),
				hash[3], hash[2], hash[1], hash[0], (current_task_time - CONFIRMATIONS_COUNT + 1) << 16 | 0xffff, res);
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
			processed = 1;
			b->field[0].transport_header = 2;

			res = xdag_add_block(b);
			if(res > 0) {
				xdag_send_new_block(b);
			}
		}

		if(!processed) sleep(1);
	}

	return 0;
}

#define calculate_diff_summ(miner, sum, count) { \
	sum = miner->prev_diff; \
	count = miner->prev_diff_count;	\
	for(int j = 0; j < CONFIRMATIONS_COUNT; ++j) {	\
		if(miner->maxdiff[j] > 0) { \
			sum += miner->maxdiff[j]; \
			++count; \
		} \
	} \
}

#define diff2pay(d, n) ((n) ? exp((d) / (n) - 20) * (n) : 0)

static inline double miner_calculate_unpaid_shares(struct miner_pool_data *miner)
{
	double sum;
	int count;
	calculate_diff_summ(miner, sum, count);
	return diff2pay(sum, count);
}

static inline double connection_calculate_unpaid_shares(struct connection_pool_data *connection)
{
	double sum;
	int count;
	calculate_diff_summ(connection, sum, count);
	return diff2pay(sum, count);
}

// calculates the rest of shares and clear shares
static double process_outdated_miner(struct miner_pool_data *miner)
{
	double sum = 0;
	int diff_count = 0;

	for(int i = 0; i < CONFIRMATIONS_COUNT; ++i) {
		if(miner->maxdiff[i] > 0) {
			sum += miner->maxdiff[i];
			miner->maxdiff[i] = 0;
			++diff_count;
		}
	}

	if(diff_count > 0) {
		sum /= diff_count;
	}

	return sum;
}

static double countpay(struct miner_pool_data *miner, int confirmation_index, double *pay)
{
	double sum = 0;
	int diff_count = 0;

	//if miner is in archive state and last connection was disconnected more than 16 minutes ago we pay for the rest of shares and clear shares
	if(miner->state == MINER_ARCHIVE && g_xdag_pool_task_index - miner->task_index > CONFIRMATIONS_COUNT) {
		sum += process_outdated_miner(miner);
		diff_count++;
	} else if(miner->maxdiff[confirmation_index] > 0) {
		sum += miner->maxdiff[confirmation_index];
		miner->maxdiff[confirmation_index] = 0;
		++diff_count;
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
	pthread_mutex_lock(&g_connections_mutex);
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

	/* clear nopaid shares for each connection */
	connection_list_element *conn;
	LL_FOREACH(g_connection_list_head, conn)
	{
		if(conn->connection_data.maxdiff[confirmation_index] > 0) {
			conn->connection_data.maxdiff[confirmation_index] = 0;
		}

		conn->connection_data.prev_diff = 0;
		conn->connection_data.prev_diff_count = 0;
	}
	pthread_mutex_unlock(&g_connections_mutex);

	if(data->sum > 0) {
		data->direct = data->balance * g_pool_direct;
		data->pay -= data->direct;
	}

	return data->prev_sum;
}

static void transfer_payment(struct miner_pool_data *miner, xdag_amount_t payment_sum, struct xdag_field *fields, int payments_per_block, int *field_index)
{
	if(payment_sum < 5) {   // payment less than 0.000000001 XDAG is ignored
		return;
	}

	memcpy(fields[*field_index].data, miner->id.data, sizeof(xdag_hashlow_t));
	fields[*field_index].amount = payment_sum;
	fields[0].amount += payment_sum;

	xdag_log_xfer(fields[0].data, fields[*field_index].data, payment_sum);

	if(++*field_index == payments_per_block) {
		xdag_create_block(fields, 1, *field_index - 1, 0, 0, NULL);
		*field_index = 1;
		fields[0].amount = 0;
	}
}

static void do_payments(uint64_t *hash, int payments_per_block, struct payment_data *data, double *diff, double *prev_diff)
{
	miner_list_element *elt;
	struct xdag_field fields[12];

	memcpy(fields[0].data, hash, sizeof(xdag_hashlow_t));
	fields[0].amount = 0;
	int field_index = 1;

	int index = 0;
	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH(g_miner_list_head, elt)
	{
		xdag_amount_t payment_sum = 0;
		struct miner_pool_data *miner = &elt->miner_data;

		if(data->prev_sum > 0) {
			payment_sum += data->pay * (prev_diff[index] / data->prev_sum);
		}

		if(data->sum > 0) {
			payment_sum += data->direct * (diff[index] / data->sum);
		}

		if(index == data->reward_index) {
			payment_sum += data->reward;
		}

		transfer_payment(miner, payment_sum, fields, payments_per_block, &field_index);
		++index;
	}
	pthread_mutex_unlock(&g_connections_mutex);

	if(g_fund_miner.state != MINER_UNKNOWN) {
		transfer_payment(&g_fund_miner, data->fund, fields, payments_per_block, &field_index);
	}

	if(field_index > 1) {
		xdag_create_block(fields, 1, field_index - 1, 0, 0, NULL);
	}
}

int pay_miners(xdag_time_t time)
{
	int defkey;
	struct payment_data data;
	miner_list_element *elt;

	memset(&data, 0, sizeof(struct payment_data));
	data.reward_index = -1;

	int miners_count;
	pthread_mutex_lock(&g_connections_mutex);
	LL_COUNT(g_miner_list_head, elt, miners_count);
	pthread_mutex_unlock(&g_connections_mutex);
	if(!miners_count) return -1;

	const int confirmation_index = time & (CONFIRMATIONS_COUNT - 1);
	uint64_t *hash = g_xdag_mined_hashes[confirmation_index];
	uint64_t *nonce = g_xdag_mined_nonce[confirmation_index];

	data.balance = xdag_get_balance(hash);
	if(!data.balance) return -2;

	data.pay = data.balance - (xdag_amount_t)(g_pool_fee * data.balance);
	if(!data.pay) return -3;

	int key = xdag_get_key(hash);
	if(key < 0) return -4;

	if(!xdag_wallet_default_key(&defkey)) return -5;

	const int payments_per_block = (key == defkey ? 12 : 10);

	int64_t pos = xdag_get_block_pos(hash, &time);
	if(pos < 0) return -6;

	struct xdag_block buf;
	struct xdag_block *block = xdag_storage_load(hash, time, pos, &buf);
	if(!block) return -7;

	double *diff = malloc(2 * miners_count * sizeof(double));
	if(!diff) return -8;
	double *prev_diff = diff + miners_count;

	if(!precalculate_payments(hash, confirmation_index, &data, diff, prev_diff, nonce)) {
		free(diff);
		return -9;
	}

	do_payments(hash, payments_per_block, &data, diff, prev_diff);

	free(diff);

	return 0;
}

void remove_inactive_miners(void)
{
	miner_list_element *elt, *eltmp;
	char address[33];

	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH_SAFE(g_miner_list_head, elt, eltmp)
	{
		if(elt->miner_data.state == MINER_ARCHIVE && miner_calculate_unpaid_shares(&elt->miner_data) == 0.0) {
			xdag_hash2address(elt->miner_data.id.data, address);

			LL_DELETE(g_miner_list_head, elt);
			clear_nonces_hashtable(&elt->miner_data);
			free(elt);

			xdag_info("Pool: miner %s is removed from miners list", address);
		}
	}
	pthread_mutex_unlock(&g_connections_mutex);
}

static const char* miner_state_to_string(int miner_state)
{
	switch(miner_state) {
		case MINER_ACTIVE:
			return "active ";
		case MINER_ARCHIVE:
			return "archive";
		case MINER_SERVICE:
			return "fee    ";
		default:
			return "unknown";
	}
}

static const char* connection_state_to_string(int connection_state)
{
	switch(connection_state) {
		case ACTIVE_CONNECTION:
			return "active ";
		default:
			return "unknown";
	}
}

static int print_miner(FILE *out, int index, struct miner_pool_data *miner, int print_connections)
{
	char ip_port_str[32], in_out_str[64];
	char address_buf[33];
	xdag_hash2address(miner->id.data, address_buf);

	fprintf(out, "%3d. %s  %s  %-21s  %-16s  %-13lf  -             %Lf\n", index, address_buf,
		miner_state_to_string(miner->state), "-", "-", miner_calculate_unpaid_shares(miner), log_difficulty2hashrate(miner->mean_log_difficulty));

	if(print_connections) {
		connection_list_element *elt;
		int conn_index = 0;
		LL_FOREACH(g_connection_list_head, elt)
		{
			if(elt->connection_data.miner == miner) {
				struct connection_pool_data *conn_data = &elt->connection_data;
				int ip = conn_data->ip;
				sprintf(ip_port_str, "%u.%u.%u.%u:%u", ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(conn_data->port));
				sprintf(in_out_str, "%llu/%llu", (unsigned long long)conn_data->nfield_in * sizeof(struct xdag_field),
					(unsigned long long)conn_data->nfield_out * sizeof(struct xdag_field));

				//TODO: fix that logic
				fprintf(out, " C%d. -                                 -        %-21s  %-16s  %-13lf  %-12s  %Lf\n", ++conn_index,
					ip_port_str, in_out_str, connection_calculate_unpaid_shares(conn_data),
					conn_data->worker_name ? conn_data->worker_name : "-", log_difficulty2hashrate(conn_data->mean_log_difficulty));
			}
		}
	}

	return miner->state == MINER_ACTIVE ? 1 : 0;
}

static int print_miners(FILE *out)
{
	pthread_mutex_lock(&g_connections_mutex);
	int count_active = print_miner(out, -1, &g_pool_miner, 1);

	miner_list_element *elt;
	int index = 0;
	LL_FOREACH(g_miner_list_head, elt)
	{
		struct miner_pool_data *miner = &elt->miner_data;
		count_active += print_miner(out, index++, miner, 1);
	}
	pthread_mutex_unlock(&g_connections_mutex);

	return count_active;
}

static void print_connection(FILE *out, int index, struct connection_pool_data *conn_data)
{
	char ip_port_str[32], in_out_str[64];
	char address[50];
	int ip = conn_data->ip;
	sprintf(ip_port_str, "%u.%u.%u.%u:%u", ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(conn_data->port));
	sprintf(in_out_str, "%llu/%llu", (unsigned long long)conn_data->nfield_in * sizeof(struct xdag_field),
		(unsigned long long)conn_data->nfield_out * sizeof(struct xdag_field));

	if(conn_data->miner) {
		xdag_hash2address(conn_data->miner->id.data, address);
	} else {
		strcpy(address, "-                               ");
	}

	//TODO: fix that logic
	fprintf(out, "%3d. %s  %s  %-21s  %-16s  %-13lf  %-12s  %Lf\n", index, address,
		connection_state_to_string(conn_data->state), ip_port_str, in_out_str, connection_calculate_unpaid_shares(conn_data),
		conn_data->worker_name ? conn_data->worker_name : "-", log_difficulty2hashrate(conn_data->mean_log_difficulty));
}

static int print_connections(FILE *out)
{
	connection_list_element *elt;
	int index = 0;
	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH(g_connection_list_head, elt)
	{
		struct connection_pool_data *conn_data = &elt->connection_data;
		print_connection(out, index++, conn_data);
	}
	pthread_mutex_unlock(&g_connections_mutex);

	return index;
}

/* output to the file a list of miners */
int xdag_print_miners(FILE *out, int printOnlyConnections)
{
	fprintf(out, "List of miners:\n"
		" NN  Address for payment to            Status   IP and port            in/out bytes      unpaid shares  worker name   hashrate MH/s\n"
		"-----------------------------------------------------------------------------------------------------------------------------------\n");

	const int count_active = printOnlyConnections ? print_connections(out) : print_miners(out);

	fprintf(out,
		"-----------------------------------------------------------------------------------------------------------------------------------\n"
		"Total %d active %s.\n", count_active, printOnlyConnections ? "connections" : "miners");

	return count_active;
}

// disconnect connections by condition
// condition type: all, ip or address
// value: address of ip depending on type
void disconnect_connections(enum disconnect_type type, char *value)
{
	connection_list_element *elt;
	xdag_hash_t hash;
	uint32_t ip = 0;

	if(type == DISCONNECT_BY_ADRESS) {
		xdag_address2hash(value, hash);
	} else if(type == DISCONNECT_BY_IP) {
		ip = inet_addr(value);
	}

	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH(g_connection_list_head, elt)
	{
		if(type == DISCONNECT_ALL) {
			elt->connection_data.deleted = 1;
			elt->connection_data.disconnection_reason = "disconnected manually";
		} else if(type == DISCONNECT_BY_ADRESS) {
			if(memcmp(elt->connection_data.data, hash, sizeof(xdag_hashlow_t)) == 0) {
				elt->connection_data.deleted = 1;
				elt->connection_data.disconnection_reason = "disconnected manually";
			}
		} else if(type == DISCONNECT_BY_IP) {
			if(elt->connection_data.ip == ip) {
				elt->connection_data.deleted = 1;
				elt->connection_data.disconnection_reason = "disconnected manually";
			}
		}
	}
	pthread_mutex_unlock(&g_connections_mutex);
}

void* pool_remove_inactive_connections(void* arg)
{
	connection_list_element *elt;

	for(;;) {
		time_t current_time = time(0);

		pthread_mutex_lock(&g_connections_mutex);
		LL_FOREACH(g_connection_list_head, elt)
		{
			if(current_time - elt->connection_data.last_share_time > 300) { //last share is received more than 5 minutes ago
				elt->connection_data.deleted = 1;
				elt->connection_data.disconnection_reason = "inactive connection";
			}
		}
		pthread_mutex_unlock(&g_connections_mutex);

		sleep(60);
	}

	return NULL;
}

void update_mean_log_diff(struct connection_pool_data *conn_data, struct xdag_pool_task *task, xdag_hash_t hash)
{
	const xdag_time_t task_time = task->task_time;

	if(conn_data->task_time < task_time) {
		if(conn_data->task_time != 0) {
			conn_data->mean_log_difficulty =
				moving_average(conn_data->mean_log_difficulty, diff2log(xdag_hash_difficulty(conn_data->last_min_hash)), conn_data->bounded_task_counter);
			if(conn_data->bounded_task_counter < NSAMPLES_MAX) {
				++conn_data->bounded_task_counter;
			}
		}
		memcpy(conn_data->last_min_hash, hash, sizeof(xdag_hash_t));
	} else if(xdag_cmphash(hash, conn_data->last_min_hash) < 0) {
		memcpy(conn_data->last_min_hash, hash, sizeof(xdag_hash_t));
	}

	if(conn_data->miner->task_time < task_time) {
		if(conn_data->miner->task_time != 0) {
			conn_data->miner->mean_log_difficulty =
				moving_average(conn_data->miner->mean_log_difficulty, diff2log(xdag_hash_difficulty(conn_data->miner->last_min_hash)), conn_data->miner->bounded_task_counter);
			if(conn_data->miner->bounded_task_counter < NSAMPLES_MAX) {
				++conn_data->miner->bounded_task_counter;
			}
		}
		memcpy(conn_data->miner->last_min_hash, hash, sizeof(xdag_hash_t));
	} else if(xdag_cmphash(hash, conn_data->miner->last_min_hash) < 0) {
		memcpy(conn_data->miner->last_min_hash, hash, sizeof(xdag_hash_t));
	}
}

long double diff2log(xdag_diff_t diff)
{
	long double res = (long double)xdag_diff_to64(diff);
	xdag_diff_shr32(&diff);
	xdag_diff_shr32(&diff);
	if(xdag_diff_to64(diff)) {
		res += ldexpl((long double)xdag_diff_to64(diff), 64);
	}
	return (res > 0 ? logl(res) : 0);
}

static void miner_print_time_intervals(struct miner_pool_data *miner, int current_interval_index, xdag_time_t current_task_time, FILE *out)
{
	char time_buf[60];

	fprintf(out, "----------------------------------------------------------------------\n");
	fprintf(out, "current  index  start time                difficulty  reward for block\n");
	fprintf(out, "----------------------------------------------------------------------\n");

	for(int i = 0; i < CONFIRMATIONS_COUNT; ++i) {
		// check if current miner mined block for current interval
		int is_reward = memcmp(g_xdag_mined_nonce[i], miner->id.data, sizeof(xdag_hashlow_t)) == 0;

		// here we calculate time offset for interval of time
		xdag_time_t task_time = current_task_time << 16 | 0xffff;
		if(i < current_interval_index) {
			task_time = task_time - (2 << 15) * (current_interval_index - i);	// 2 << 15 - 64 seconds
		} else if(i > current_interval_index) {
			task_time = task_time - (2 << 15) * (current_interval_index + CONFIRMATIONS_COUNT - i);
		}
		xdag_time_to_string(task_time, time_buf);

		fprintf(out, "      %s  %2d     %s  %10lf         %s\n",
			i == current_interval_index ? ">" : " ", i + 1, time_buf, miner->maxdiff[i], is_reward ? "+" : " ");
	}
}

static void connection_print_time_intervals(struct connection_pool_data *conn_data, int current_interval_index, FILE *out)
{
	fprintf(out, "--------------------------\n");
	fprintf(out, "current  index  difficulty\n");
	fprintf(out, "--------------------------\n");

	for(int i = 0; i < CONFIRMATIONS_COUNT; ++i) {
		fprintf(out, "      %s  %2d    %10lf\n",
			i == current_interval_index ? ">" : " ", i + 1, conn_data->maxdiff[i]);
	}
}

static void print_connection_stats(struct connection_pool_data *conn_data, int connection_index, int current_interval_index, FILE *out)
{
	char time_buf[50];
	time_to_string(conn_data->connected_time, time_buf);
	int ip = conn_data->ip;

	fprintf(out, "\nConnection %d\n", connection_index);
	fprintf(out, "IP and port: %u.%u.%u.%u:%u\n", ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(conn_data->port));
	fprintf(out, "Connected at: %s\n", time_buf);
	fprintf(out, "In/out data: %llu/%llu\n", (unsigned long long)conn_data->nfield_in * sizeof(struct xdag_field),
		(unsigned long long)conn_data->nfield_out * sizeof(struct xdag_field));
	if(conn_data->worker_name) {
		fprintf(out, "Worker name: %s\n", conn_data->worker_name);
	}
	fprintf(out, "Unpaid shares rate: %lf\n", connection_calculate_unpaid_shares(conn_data));
	fprintf(out, "Approximate hashrate: %Lf\n", log_difficulty2hashrate(conn_data->mean_log_difficulty));
	if(conn_data->prev_diff_count > 0) {
		fprintf(out, "Outdated shares (indirect contribution):\n");
		fprintf(out, "Summ of difficulties: %lf\n", conn_data->prev_diff);
		fprintf(out, "Count of shares: %d\n", conn_data->prev_diff_count);
	}

	fprintf(out, "Time intervals:\n");
	connection_print_time_intervals(conn_data, current_interval_index, out);

	double total_difficulty_summ;
	int total_difficulty_count;
	calculate_diff_summ(conn_data, total_difficulty_summ, total_difficulty_count);
	fprintf(out, "Total summ of difficulties: %lf\n", total_difficulty_summ);
	fprintf(out, "Total count of difficulties: %d\n", total_difficulty_count);
}

static void print_miner_stats(struct miner_pool_data *miner, FILE *out)
{
	char time_buf[50];
	time_to_string(miner->registered_time, time_buf);

	const uint64_t task_index = g_xdag_pool_task_index;
	struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];
	const xdag_time_t current_task_time = task->task_time;
	const int current_interval_index = current_task_time & (CONFIRMATIONS_COUNT - 1);

	uint64_t *h = miner->id.data;
	fprintf(out, "Hash: %016llx%016llx%016llx%016llx\n", 
		(unsigned long long)h[3], (unsigned long long)h[2], (unsigned long long)h[1], (unsigned long long)h[0]);
	fprintf(out, "Registered at: %s\n", time_buf);
	fprintf(out, "State: %s\n", miner_state_to_string(miner->state));
	fprintf(out, "Unpaid shares rate: %lf\n", miner_calculate_unpaid_shares(miner));
	fprintf(out, "Approximate hashrate: %Lf\n", log_difficulty2hashrate(miner->mean_log_difficulty));
	if(miner->prev_diff_count > 0) {
		fprintf(out, "Outdated shares (indirect contribution):\n");
		fprintf(out, "Summ of difficulties: %lf\n", miner->prev_diff);
		fprintf(out, "Count of difficulties: %d\n", miner->prev_diff_count);
	}

	fprintf(out, "Time intervals:\n");
	miner_print_time_intervals(miner, current_interval_index, current_task_time, out);

	double total_difficulty_summ;
	int total_difficulty_count;
	calculate_diff_summ(miner, total_difficulty_summ, total_difficulty_count);
	fprintf(out, "Total summ of difficulties: %lf\n", total_difficulty_summ);
	fprintf(out, "Total count of difficulties: %d\n", total_difficulty_count);

	connection_list_element *elt;
	int index = 1;
	LL_FOREACH(g_connection_list_head, elt)
	{
		if(elt->connection_data.miner == miner) {
			struct connection_pool_data *conn_data = &elt->connection_data;
			print_connection_stats(conn_data, index++, current_interval_index, out);
		}
	}
}

// prints detailed information about specified miner
int xdag_print_miner_stats(const char* address, FILE *out)
{
	miner_list_element *elt;
	xdag_hash_t hash;
	xdag_address2hash(address, hash);

	int exists = 0;
	pthread_mutex_lock(&g_connections_mutex);
	LL_FOREACH(g_miner_list_head, elt)
	{
		if(memcmp(elt->miner_data.id.data, hash, sizeof(xdag_hashlow_t)) == 0) {
			struct miner_pool_data *miner = &elt->miner_data;
			exists = 1;
			print_miner_stats(miner, out);

			break;
		}
	}
	pthread_mutex_unlock(&g_connections_mutex);

	return exists;
}
