// pool logic

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "block.h"
#include "sync.h"
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

struct miner_pool_data {
	struct xdag_field id;
	xdag_time_t task_time;
	double prev_diff;
	uint32_t prev_diff_count;
	double maxdiff[CONFIRMATIONS_COUNT];

	//uint32_t shares_count;
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
	uint16_t state;
	uint8_t data_size;
	uint8_t block_size;
	struct pollfd connection_descriptor;
};

typedef struct connection_list_element {
	struct connection_pool_data connection_data;
	struct connection_list_element *next;
} connection_list_element;

xdag_hash_t g_xdag_mined_hashes[CONFIRMATIONS_COUNT], g_xdag_mined_nonce[CONFIRMATIONS_COUNT];

static int g_max_miners_count = START_MINERS_COUNT, g_max_miner_ip_count = START_MINERS_IP_COUNT;
static int g_miners_count = 0;

connection_list_element *g_connection_list_head = NULL;

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
		++g_miners_count;

		xdag_info("Pool  : miner %d connected from %u.%u.%u.%u:%u", g_miners_count,
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, ntohs(new_connection->connection_data.port));
	}

	return 0;
}
