/* пул и майнер, T13.744-T13.000 $DVS:time$ */

#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "system.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_string.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "block.h"
#include "pool.h"
#include "log.h"

#define N_MINERS		4096
#define N_ARCHIVE		4096
#define N_CONFIRMATIONS	16
#define MINERS_PWD		"minersgonnamine"
#define SECTOR0_BASE	0x1947f3ac
#define SECTOR0_OFFSET	0x82e9d1b5
#define HEADER_WORD		0x3fca9e2b
#define DATA_SIZE		(sizeof(struct cheatcoin_field) / sizeof(uint32_t))

enum miner_state {
	MINER_BLOCK		= 1,
	MINER_ARCHIVE	= 2,
	MINER_FREE		= 4,
	MINER_BALANCE	= 8,
};

struct miner {
	double minhash[N_CONFIRMATIONS];
	struct cheatcoin_field id;
	uint32_t data[DATA_SIZE];
	cheatcoin_time_t main_time;
	uint64_t nfield_in;
	uint64_t nfield_out;
	uint64_t ntask;
	struct cheatcoin_block *block;
	uint16_t state;
	uint8_t data_size;
	uint8_t block_size;
};

struct cheatcoin_pool_task g_cheatcoin_pool_task[2];
int g_cheatcoin_pool_ntask;
int g_cheatcoin_mining_threads = 0;

/* 1 - программа работает как пул */
static int g_cheatcoin_pool = 0;
static double g_pool_fee = 0, g_pool_reward = 0;
static struct miner *g_miners;
static struct pollfd *g_fds;
static int g_nminers = 0;
static struct dfslib_crypt *g_crypt;
static struct block *g_firstb = 0, *g_lastb = 0;
static struct pthread_mutex_t g_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_stop_mining = 1, g_stop_general_mining = 1;
static struct miner g_local_miner;

static inline void set_share(struct miner *m, struct cheatcoin_pool_task *task, cheatcoin_hash_t last, cheatcoin_hash_t hash) {
	cheatcoin_time_t t = task->main_time;
	if (cheatcoin_cmphash(hash, task->minhash) < 0) {
		memcpy(task->minhash, hash, sizeof(cheatcoin_hash_t));
		memcpy(task->lastfield, last, sizeof(struct cheatcoin_field));
	}
	if (m->main_time <= t) {
		double hashd = ((uint64_t *)hash)[2];
		int i = t & (N_CONFIRMATIONS - 1);
		hashd = ldexp(hashd, -64);
		hashd += ((uint64_t *)hash)[3];
		hashd = ldexp(hashd, -32);
		if (m->main_time < t) {
			m->main_time = t;
			m->minhash[i] = hashd;
			m->state &= ~MINER_BALANCE;
		} else if (hashd < m->minhash[i]) m->minhash[i] = hashd;
	}
}

static void *pool_main_thread(void *arg) {
	struct miner *m;
	int i, todo, nminers;
	cheatcoin_time_t t;
	for(;;) {
		nminers = g_nminers;
		if (!poll(g_fds, nminers, 1000)) continue;
		for (i = 0; i < nminers; ++i) {
			m = miners + i;
			if (m->state & (MINER_ARCHIVE | MINER_FREE)) continue;
			if (g_fds[i].revents & POLLIN) {
				todo = sizeof(struct cheatcoin_field) - m->data_size;
				todo = read(g_fds[i].socket, (uint8_t *)m->data + m->data_size, todo);
				if (todo <= 0) {
					close(g_fds[i].socket);
					if (m->block) free(m->block);
					g_fds[i].socket = 0;
					g_fds[i].events = 0;
					m->state |= MINER_ARCHIVE;
					m->last = t;
					continue;
				}
				m->data_size += todo;
				if (m->data_size == sizeof(struct cheatcoin_field)) {
					m->data_size = 0;
					dfslib_uncrypt_array(crypt, data, DATA_SIZE, m->nfield_in++);
					if (!m->block_size && data[0] == HEADER_WORD) {
						m->block = malloc(sizeof(struct cheatcoin_block));
						if (!m->block) continue;
						memcpy(m->block->field, data, sizeof(struct cheatcoin_field));
						m->block_size++;
					} else if (m->nfield_in == 1) {
						close(g_fds[i].socket);
						if (m->block) free(m->block);
						g_fds[i].socket = 0;
						g_fds[i].events = 0;
						m->state |= MINER_FREE;
					} else if (m->block_size) {
						memcpy(m->block->field + m->block_size, data, sizeof(struct cheatcoin_field));
						m->block_size++;
						if (m->block_size == CHEATCOIN_BLOCK_FIELDS) {
							uint32_t crc = ((uint32_t *)m->block)[1];
							((uint32_t *)m->block)[1] = 0;
							if (crc == crc_of_array((char *)m->block, sizeof(struct cheatcoin_block))) {
								*(uintptr_t *)m->block = 0;
								pthread_mutex_lock(&g_list_mutex);
								if (!g_firstb) g_firstb = g_lastb = m->block;
								else *(uintptr_t *)g_lastb = m->block, g_lastb = m->block;
								pthread_mutex_unlock(&g_list_mutex);
							} else free(m->block);
							m->block = 0;
							m->block_size = 0;
						}
					} else {
						int ntask = g_cheatcoin_pool_ntask;
						struct cheatcoin_pool_task *task = &g_cheatcoin_pool_task[ntask & 1];
						cheatcoin_hash_t hash;
						memcpy(m->id.data, m->data, sizeof(struct cheatcoin_field));
						cheatcoin_hash_final(task->ctx0, m->data, sizeof(struct cheatcoin_field), hash);
						set_share(m, task, m->data, hash);
					}
				}
			}
			if (g_fds[i].revents & POLLOUT) {
				struct cheatcoin_field data[2];
				uint64_t ntask = g_cheatcoin_pool_ntask;
				int i, nfld = 0;
				if (m->ntask < ntask) {
					m->ntask = ntask;
					nfld = 2;
					memcpy(data, g_cheatcoin_pool_task[ntask & 1].task, nfld * sizeof(struct cheatcoin_field_t));
				} else if (!(m->state & MINER_BALANCE)) {

				}
			}
		}
	}
}

static void *pool_net_thread(void *arg) {
	const char *str = (const char *)arg;
	char buf[0x100];
	const char *mess, *mess1 = "";
	struct sockaddr_in peeraddr;
	struct hostent *host;
	char *lasts;
	int res = 0, sock, fd, rcvbufsize = 1024, reuseaddr = 1, i;
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	socklen_t peeraddr_len = sizeof(peeraddr);
	cheatcoin_time_t t;
	struct miner *m;

	// Create a socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) { mess = "cannot create a socket"; goto err; }
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		cheatcoin_err("pool  : can't set FD_CLOEXEC flag on socket %d, %s\n", sock, strerror(errno));
	}

	// Fill in the address of server
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, str);
	str = strtok_r(buf, " \t\r\n:", &lasts);
	if (!str) { mess = "host is not given"; goto err; }
	if (!strcmp(str, "any")) {
		peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		host = gethostbyname(str);
		if (!host || !host->h_addr_list[0]) { mess = "cannot resolve host ", mess1 = str; res = h_errno; goto err; }
		// Write resolved IP address of a server to the address structure
		memmove(&peeraddr.sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// Resolve port
	str = strtok_r(0, " \t\r\n:", &lasts);
	if (!str) { mess = "port is not given"; goto err; }
	peeraddr.sin_port = htons(atoi(str));

	res = bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
	if (res) { mess = "cannot bind a socket"; goto err; }

	// Set the "LINGER" timeout to zero, to close the listen socket
	// immediately at program termination.
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbufsize, sizeof(int));

	// Now, listen for a connection
	res = listen(sock, N_MINERS);    // "1" is the maximal length of the queue
	if (res) { mess = "cannot listen"; goto err; }

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) sscanf(str, "%lf", &g_pool_fee);
	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) sscanf(str, "%lf", &g_pool_reward);

	for(;;) {
		// Accept a connection (the "accept" command waits for a connection with
		// no timeout limit...)
		fd = accept(sock, (struct sockaddr*) &peeraddr, &peeraddr_len);
		if (fd < 0) { mess = "cannot accept"; goto err; }

		for (i = 0; i < g_nminers; ++i) {
			m = g_miners + i;
			if (m->state & MINER_FREE) break;
			if (m->state & MINER_ARCHIVE && t - m->main_time > N_CONFIRMATIONS) break;
		}
		if (i >= N_MINERS) {
			fclose(fd);
		} else {
			m = g_miners + i;
			g_fds[i].socket = fd;
			g_fds[i].events = POLLIN | POLLOUT;
			g_fds[i].revents = 0;
			memset(m, 0, sizeof(struct miner));
			if (i == g_nminers) g_nminers++;
		}
	}

	return 0;
err:
	cheatcoin_err("pool  : %s %s (error %d)", mess, mess1, res);
	return 0;
}

static int crypt_start(void) {
	struct dfslib_string str;
	uint32_t sector0[128];
	int i;
	crypt = malloc(sizeof(struct dfslib_crypt));
	if (!crypt) return -1;
	dfslib_crypt_set_password(crypt, dfslib_utf8_string(str, MINERS_PWD, strlen(MINERS_PWD)));
	for (i = 0; i < 128; ++i) sector0[i] = SECTOR0_BASE + i * SECTOR0_OFFSET;
	for (i = 0; i < 128; ++i) {
		dfslib_crypt_set_sector0(crypt, sector0);
		dfslib_encrypt_sector(crypt, sector0, SECTOR0_BASE + i * SECTOR0_OFFSET);
	}
	return 0;
}

static void *mining_thread(void *arg) {
	struct cheatcoin_pool_task *task;
	cheatcoin_hash_t hash;
	struct cheatcoin_field last;
	int nthread = (int)(uintptr_t)arg;
	int ntask, oldntask = 0;
	uint64_t nonce;
	while (!g_cheatcoin_sync_on && !g_stop_mining) sleep(1);
	while (!g_stop_mining) {
		ntask = g_cheatcoin_pool_ntask;
		task = g_cheatcoin_pool_task[ntask & 1];
		if (!ntask) { sleep(1); continue; }
		if (ntask != oldntask) {
			memcpy(last.data, task->nonce.data, sizeof(cheatcoin_hash_t));
			nonce = last.amount + nthread;
		} else nonce += g_cheatcoin_mining_threads;
		last.amount = cheatcoin_hash_final_multi(task->ctx, &nonce, 256, hash);
		set_share(&g_local_miner, task, last.data, hash);
	}
	return 0;
}

static void *general_mining_thread(void *arg) {
	while (!g_cheatcoin_sync_on && !g_stop_general_mining) sleep(1);
	while (!g_stop_general_mining) cheatcoin_create_block(0, 0, 0, 0, cheatcoin_main_time() << 16 | 0xffff);
	cheatcoin_mess("Stopping general mining thread...");
	return 0;
}

int cheatcoin_mining_start(int n_mining_threads) {
	pthread_t th;
	if ((n_mining_threads || g_cheatcoin_pool) && g_stop_general_mining) {
		cheatcoin_mess("Starting general mining thread...");
		g_stop_general_mining = 0;
		pthread_create(&th, 0, general_mining_thread, 0);
		pthread_detach(th);
	}
	if (n_mining_threads == g_cheatcoin_mining_threads);
	else if (!n_mining_threads) {
		g_stop_mining = 1;
		if (!g_cheatcoin_pool) g_stop_general_mining = 1;
		g_cheatcoin_mining_threads = 0;
	} else if (!g_cheatcoin_mining_threads) {
		g_stop_mining = 0;
	} else if (g_cheatcoin_mining_threads > n_mining_threads) {
		g_stop_mining = 1;
		sleep(5);
		g_stop_mining = 0;
		g_cheatcoin_mining_threads = 0;
	}
	while (g_cheatcoin_mining_threads < n_mining_threads) {
		pthread_create(&th, 0, mining_thread, (void *)(uintptr_t)g_cheatcoin_mining_threads);
		pthread_detach(th);
		g_cheatcoin_mining_threads++;
	}
	return 0;
}

int cheatcoin_pool_start(int pool_on, const char *pool_arg) {
	pthread_t th;
	int i, res;
	g_cheatcoin_pool = pool_on;
	for (i = 0; i < 2; ++i) {
		g_cheatcoin_pool_task[i].ctx0 = malloc(cheatcoin_hash_ctx_size());
		g_cheatcoin_pool_task[i].ctx = malloc(cheatcoin_hash_ctx_size());
		if (!g_cheatcoin_pool_task[i].ctx0 || !g_cheatcoin_pool_task[i].ctx) return -1;
	}
	if (!pool_on) return 0;
	g_miners = malloc(N_MINERS * sizeof(struct miner));
	if (!g_miners) return -1;
	if (crypt_start()) return -1;
	res = pthread_create(&th, 0, pool_main_thread, 0);
	if (res) return -1;
	pthread_detach(th);
	res = pthread_create(&th, 0, pool_net_thread, 0);
	if (res) return -1;
	pthread_detach(th);
	return 0;
}
