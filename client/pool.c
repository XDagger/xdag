/* пул и майнер, T13.744-T13.837 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#if defined(_WIN32) || defined(_WIN64)
#if defined(_WIN64)
#define poll WSAPoll
#else
#define poll(a,b,c) ((a)->revents = (a)->events, (b))
#endif
#else
#include <poll.h>
#endif
#include "system.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_string.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "address.h"
#include "block.h"
#include "main.h"
#include "pool.h"
#include "storage.h"
#include "sync.h"
#include "transport.h"
#include "wallet.h"
#include "log.h"

#define N_MINERS		4096
#define START_N_MINERS	256
#define START_N_MINERS_IP 8
#define N_CONFIRMATIONS	CHEATCOIN_POOL_N_CONFIRMATIONS
#define MINERS_PWD		"minersgonnamine"
#define SECTOR0_BASE	0x1947f3acu
#define SECTOR0_OFFSET	0x82e9d1b5u
#define HEADER_WORD		0x3fca9e2bu
#define DATA_SIZE		(sizeof(struct cheatcoin_field) / sizeof(uint32_t))
#define SEND_PERIOD		10 /* период в секундах, с которым майнер посылает пулу результаты */
#define FUND_ADDRESS	"FQglVQtb60vQv2DOWEUL7yh3smtj7g1s" /* адрес фонда сообщества */

enum miner_state {
	MINER_BLOCK		= 1,
	MINER_ARCHIVE	= 2,
	MINER_FREE		= 4,
	MINER_BALANCE	= 8,
	MINER_ADDRESS	= 0x10,
};

struct miner {
	double maxdiff[N_CONFIRMATIONS];
	struct cheatcoin_field id;
	uint32_t data[DATA_SIZE];
	double prev_diff;
	cheatcoin_time_t main_time;
	uint64_t nfield_in;
	uint64_t nfield_out;
	uint64_t ntask;
	struct cheatcoin_block *block;
	uint32_t ip;
	uint32_t prev_diff_count;
	uint16_t port;
	uint16_t state;
	uint8_t data_size;
	uint8_t block_size;
};

struct cheatcoin_pool_task g_cheatcoin_pool_task[2];
uint64_t g_cheatcoin_pool_ntask;
int g_cheatcoin_mining_threads = 0;
cheatcoin_hash_t g_cheatcoin_mined_hashes[N_CONFIRMATIONS], g_cheatcoin_mined_nonce[N_CONFIRMATIONS];

/* 1 - программа работает как пул */
static int g_cheatcoin_pool = 0, g_max_nminers = START_N_MINERS, g_max_nminers_ip = START_N_MINERS_IP, g_nminers = 0, g_socket = -1,
		g_stop_mining = 1, g_stop_general_mining = 1;
static double g_pool_fee = 0, g_pool_reward = 0, g_pool_direct = 0, g_pool_fund = 0;
static struct miner *g_miners, g_local_miner, g_fund_miner;
static struct pollfd *g_fds;
static struct dfslib_crypt *g_crypt;
static struct cheatcoin_block *g_firstb = 0, *g_lastb = 0;
static pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_share_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *g_miner_address;
void *g_ptr_share_mutex = &g_share_mutex;

static inline void set_share(struct miner *m, struct cheatcoin_pool_task *task, cheatcoin_hash_t last, cheatcoin_hash_t hash) {
	cheatcoin_time_t t = task->main_time;
	if (cheatcoin_cmphash(hash, task->minhash.data) < 0) {
		pthread_mutex_lock(&g_share_mutex);
		if (cheatcoin_cmphash(hash, task->minhash.data) < 0) {
			memcpy(task->minhash.data, hash, sizeof(cheatcoin_hash_t));
			memcpy(task->lastfield.data, last, sizeof(cheatcoin_hash_t));
		}
		pthread_mutex_unlock(&g_share_mutex);
	}
	if (m->main_time <= t) {
		double diff = ((uint64_t *)hash)[2];
		int i = t & (N_CONFIRMATIONS - 1);
		diff = ldexp(diff, -64);
		diff += ((uint64_t *)hash)[3];
		if (diff < 1) diff = 1;
		diff = 46 - log(diff);
		if (m->main_time < t) {
			m->main_time = t;
			if (m->maxdiff[i] > 0) {
				m->prev_diff += m->maxdiff[i];
				m->prev_diff_count++;
			}
			m->maxdiff[i] = diff;
			m->state &= ~MINER_BALANCE;
		} else if (diff > m->maxdiff[i]) m->maxdiff[i] = diff;
	}
}

static void *pool_main_thread(void *arg) {
	struct cheatcoin_pool_task *task;
	const char *mess;
	struct miner *m;
	struct pollfd *p;
	uint64_t ntask;
	int i, todo, nminers, done, j;

	while (!g_cheatcoin_sync_on) sleep(1);

	for(;;) {
		nminers = g_nminers;
		if (!poll(g_fds, nminers, 1000)) continue;
		for (i = done = 0; i < nminers; ++i) {
			m = g_miners + i;
			p = g_fds + i;
			if (m->state & (MINER_ARCHIVE | MINER_FREE)) continue;
			if (p->revents & POLLNVAL) continue;
			if (p->revents & POLLHUP) {
				done = 1; mess = "socket hangup";
			disconnect:
				m->state |= MINER_ARCHIVE;
			disconnect_free:
				close(p->fd);
				p->fd = -1;
				if (m->block) { free(m->block); m->block = 0; }
				j = m->ip;
				cheatcoin_info("Pool  : miner %d disconnected from %u.%u.%u.%u:%u by %s", i,
					j & 0xff, j >> 8 & 0xff, j >> 16 & 0xff, j >> 24 & 0xff, ntohs(m->port), mess);
				continue;
			}
			if (p->revents & POLLERR) { done = 1; mess = "socket error"; goto disconnect; }
			if (p->revents & POLLIN) {
				done = 1;
				todo = sizeof(struct cheatcoin_field) - m->data_size;
				todo = read(p->fd, (uint8_t *)m->data + m->data_size, todo);
				if (todo <= 0) { mess = "read error"; goto disconnect; }
				m->data_size += todo;
				if (m->data_size == sizeof(struct cheatcoin_field)) {
					m->data_size = 0;
					dfslib_uncrypt_array(g_crypt, m->data, DATA_SIZE, m->nfield_in++);
					if (!m->block_size && m->data[0] == HEADER_WORD) {
						m->block = malloc(sizeof(struct cheatcoin_block));
						if (!m->block) continue;
						memcpy(m->block->field, m->data, sizeof(struct cheatcoin_field));
						m->block_size++;
					} else if (m->nfield_in == 1) {
						mess = "protocol mismatch"; m->state = MINER_FREE; goto disconnect_free;
					} else if (m->block_size) {
						memcpy(m->block->field + m->block_size, m->data, sizeof(struct cheatcoin_field));
						m->block_size++;
						if (m->block_size == CHEATCOIN_BLOCK_FIELDS) {
							uint32_t crc = m->block->field[0].transport_header >> 32;
							m->block->field[0].transport_header &= (uint64_t)0xffffffffu;
							if (crc == crc_of_array((uint8_t *)m->block, sizeof(struct cheatcoin_block))) {
								m->block->field[0].transport_header = 0;
								pthread_mutex_lock(&g_pool_mutex);
								if (!g_firstb) g_firstb = g_lastb = m->block;
								else g_lastb->field[0].transport_header = (uintptr_t)m->block, g_lastb = m->block;
								pthread_mutex_unlock(&g_pool_mutex);
							} else free(m->block);
							m->block = 0;
							m->block_size = 0;
						}
					} else {
						cheatcoin_hash_t hash;
						ntask = g_cheatcoin_pool_ntask;
						task = &g_cheatcoin_pool_task[ntask & 1];
						if (!(m->state & MINER_ADDRESS) || memcmp(m->id.data, m->data, sizeof(cheatcoin_hashlow_t))) {
							cheatcoin_time_t t;
							int64_t pos;
							memcpy(m->id.data, m->data, sizeof(struct cheatcoin_field));
							pos = cheatcoin_get_block_pos(m->id.data, &t);
							if (pos < 0) m->state &= ~MINER_ADDRESS;
							else m->state |= MINER_ADDRESS;
						} else memcpy(m->id.data, m->data, sizeof(struct cheatcoin_field));
						cheatcoin_hash_final(task->ctx0, m->data, sizeof(struct cheatcoin_field), hash);
						set_share(m, task, m->id.data, hash);
					}
				}
			}
			if (p->revents & POLLOUT) {
				struct cheatcoin_field data[2];
				int j, nfld = 0;
				ntask = g_cheatcoin_pool_ntask;
				task = &g_cheatcoin_pool_task[ntask & 1];
				if (m->ntask < ntask) {
					m->ntask = ntask;
					nfld = 2;
					memcpy(data, task->task, nfld * sizeof(struct cheatcoin_field));
				} else if (!(m->state & MINER_BALANCE)) {
					m->state |= MINER_BALANCE;
					memcpy(data[0].data, m->id.data, sizeof(cheatcoin_hash_t));
					data[0].amount = cheatcoin_get_balance(data[0].data);
					nfld = 1;
				}
				if (nfld) {
					done = 1;
					for (j = 0; j < nfld; ++j)
						dfslib_encrypt_array(g_crypt, (uint32_t *)(data + j), DATA_SIZE, m->nfield_out++);
					todo = write(p->fd, data, nfld * sizeof(struct cheatcoin_field));
					if (todo != nfld * sizeof(struct cheatcoin_field)) { mess = "write error"; goto disconnect; }
				}
			}
		}
		if (!done) sleep(1);
	}
	return 0;
}

#define diff2pay(d,n) ((n) ? exp((d) / (n) - 20) * (n) : 0)

static double countpay(struct miner *m, int i, double *pay) {
	double sum = 0;
	int n = 0;
	if (m->maxdiff[i] > 0) { sum += m->maxdiff[i]; m->maxdiff[i] = 0; n++; }
	*pay = diff2pay(sum, n);
	sum += m->prev_diff;
	n += m->prev_diff_count;
	m->prev_diff = 0;
	m->prev_diff_count = 0;
	return diff2pay(sum, n);
}

static int pay_miners(cheatcoin_time_t t) {
	struct cheatcoin_field fields[12];
	struct cheatcoin_block buf, *b;
	uint64_t *h, *nonce;
	cheatcoin_amount_t balance, pay, reward = 0, direct = 0, fund = 0;
	struct miner *m;
	int64_t pos;
	int i, n, nminers, reward_ind = -1, key, defkey, nfields, nfld;
	double *diff, *prev_diff, sum, prev_sum, topay;
	nminers = g_nminers;
	if (!nminers) return -1;
	n = t & (N_CONFIRMATIONS - 1);
	h = g_cheatcoin_mined_hashes[n];
	nonce = g_cheatcoin_mined_nonce[n];
	balance = cheatcoin_get_balance(h);
	if (!balance) return -2;
	pay = balance - g_pool_fee * balance;
	if (!pay) return -3;
	reward = balance * g_pool_reward;
	pay -= reward;
	if (g_pool_fund) {
		if (!(g_fund_miner.state & MINER_ADDRESS)) {
			cheatcoin_time_t t;
			if (!cheatcoin_address2hash(FUND_ADDRESS, g_fund_miner.id.hash) && cheatcoin_get_block_pos(g_fund_miner.id.hash, &t) >= 0)
				g_fund_miner.state |= MINER_ADDRESS;
		}
		if (g_fund_miner.state & MINER_ADDRESS) {
			fund = balance * g_pool_fund;
			pay -= fund;
		}
	}
	key = cheatcoin_get_key(h);
	if (key < 0) return -4;
	if (!cheatcoin_wallet_default_key(&defkey)) return -5;
	nfields = (key == defkey ? 12 : 10);
	pos = cheatcoin_get_block_pos(h, &t);
	if (pos < 0) return -6;
	b = cheatcoin_storage_load(h, t, pos, &buf);
	if (!b) return -7;
	diff = malloc(2 * nminers * sizeof(double));
	if (!diff) return -8;
	prev_diff = diff + nminers;
	prev_sum = countpay(&g_local_miner, n, &sum);
	for (i = 0; i < nminers; ++i) {
		m = g_miners + i;
		if (m->state & MINER_FREE || !(m->state & MINER_ADDRESS)) continue;
		prev_diff[i] = countpay(m, n, &diff[i]);
		sum += diff[i];
		prev_sum += prev_diff[i];
		if (reward_ind < 0 && !memcmp(nonce, m->id.data, sizeof(cheatcoin_hashlow_t))) reward_ind = i;
	}
	if (!prev_sum) return -9;
	if (sum > 0) {
		direct = balance * g_pool_direct;
		pay -= direct;
	}
	memcpy(fields[0].data, h, sizeof(cheatcoin_hashlow_t));
	fields[0].amount = 0;
	for (nfld = 1, i = 0; i <= nminers; ++i) {
		if (i < nminers) {
			m = g_miners + i;
			if (m->state & MINER_FREE || !(m->state & MINER_ADDRESS)) continue;
			topay = pay * (prev_diff[i] / prev_sum);
			if (sum > 0) topay += direct * (diff[i] / sum);
			if (i == reward_ind) topay += reward;
		} else {
			m = &g_fund_miner;
			if (!(m->state & MINER_ADDRESS)) continue;
			topay = fund;
		}
		if (!topay) continue;
		memcpy(fields[nfld].data, m->id.data, sizeof(cheatcoin_hashlow_t));
		fields[nfld].amount = topay;
		fields[0].amount += topay;
		cheatcoin_log_xfer(fields[0].data, fields[nfld].data, topay);
		if (++nfld == nfields) {
			cheatcoin_create_block(fields, 1, nfld - 1, 0, 0);
			nfld = 1;
			fields[0].amount = 0;
		}
	}
	if (nfld > 1) cheatcoin_create_block(fields, 1, nfld - 1, 0, 0);

	return 0;
}

static void *pool_block_thread(void *arg) {
	struct cheatcoin_pool_task *task;
	uint64_t ntask;
	cheatcoin_time_t t0 = 0, t;
	struct cheatcoin_block *b;
	int done, res;

	while (!g_cheatcoin_sync_on) sleep(1);

	for(;;) {
		done = 0;
		ntask = g_cheatcoin_pool_ntask;
		task = &g_cheatcoin_pool_task[ntask & 1];
		t = task->main_time;
		if (t > t0) {
			uint64_t *h = g_cheatcoin_mined_hashes[(t - N_CONFIRMATIONS + 1) & (N_CONFIRMATIONS - 1)];
			done = 1;
			t0 = t;
			res = pay_miners(t - N_CONFIRMATIONS + 1);
			cheatcoin_info("%s: %016llx%016llx%016llx%016llx t=%llx res=%d", (res ? "Nopaid" : "Paid  "),
				h[3], h[2], h[1], h[0], (t - N_CONFIRMATIONS + 1) << 16 | 0xffff, res);
		}
		pthread_mutex_lock(&g_pool_mutex);
		if (g_firstb) {
			b = g_firstb;
			g_firstb = (struct cheatcoin_block *)(uintptr_t)b->field[0].transport_header;
			if (!g_firstb) g_lastb = 0;
		} else b = 0;
		pthread_mutex_unlock(&g_pool_mutex);
		if (b) {
			done = 1;
			b->field[0].transport_header = 2;
			res = cheatcoin_add_block(b);
			if (res > 0) cheatcoin_send_new_block(b);
		}
		if (!done) sleep(1);
	}
	return 0;
}

char *cheatcoin_pool_get_config(char *buf) {
	if (!g_cheatcoin_pool) return 0;
	sprintf(buf, "%d:%.2lf:%.2lf:%.2lf:%d:%.2lf", g_max_nminers, g_pool_fee * 100, g_pool_reward * 100,
	    g_pool_direct * 100, g_max_nminers_ip, g_pool_fund * 100);
	return buf;
}

int cheatcoin_pool_set_config(const char *str) {
	char buf[0x100], *lasts;
	if (!g_cheatcoin_pool) return -1;
	strcpy(buf, str);

	str = strtok_r(buf, " \t\r\n:", &lasts);
	if (str) {
		int open_max = sysconf(_SC_OPEN_MAX);
		sscanf(str, "%d", &g_max_nminers);
		if (g_max_nminers < 0) g_max_nminers = 0;
		else if (g_max_nminers > N_MINERS) g_max_nminers = N_MINERS;
		else if (g_max_nminers > open_max - 64) g_max_nminers = open_max - 64;
	}

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) {
		sscanf(str, "%lf", &g_pool_fee);
		g_pool_fee /= 100;
		if (g_pool_fee < 0) g_pool_fee = 0;
		if (g_pool_fee > 1) g_pool_fee = 1;
	}

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) {
		sscanf(str, "%lf", &g_pool_reward);
		g_pool_reward /= 100;
		if (g_pool_reward < 0) g_pool_reward = 0;
		if (g_pool_fee + g_pool_reward > 1) g_pool_reward = 1 - g_pool_fee;
	}

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) {
		sscanf(str, "%lf", &g_pool_direct);
		g_pool_direct /= 100;
		if (g_pool_direct < 0) g_pool_direct = 0;
		if (g_pool_fee + g_pool_reward + g_pool_direct > 1) g_pool_direct = 1 - g_pool_fee - g_pool_reward;
	}

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) {
		sscanf(str, "%d", &g_max_nminers_ip);
		if (g_max_nminers_ip <= 0) g_max_nminers_ip = 1;
	}

	str = strtok_r(0, " \t\r\n:", &lasts);
	if (str) {
		sscanf(str, "%lf", &g_pool_fund);
		g_pool_fund /= 100;
		if (g_pool_fund < 0) g_pool_fund = 0;
		if (g_pool_fee + g_pool_reward + g_pool_direct + g_pool_fund > 1) g_pool_fund = 1 - g_pool_fee - g_pool_reward - g_pool_direct;
	}

	return 0;
}

static void *pool_net_thread(void *arg) {
	const char *str = (const char *)arg;
	char buf[0x100];
	const char *mess, *mess1 = "";
	struct sockaddr_in peeraddr;
//	struct hostent *host;
	char *lasts;
	int res = 0, sock, fd, rcvbufsize = 1024, reuseaddr = 1, i, j, i0, count;
//	unsigned long nonblock = 1;
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	socklen_t peeraddr_len = sizeof(peeraddr);
	cheatcoin_time_t t;
	struct miner *m;
	struct pollfd *p;

	while (!g_cheatcoin_sync_on) sleep(1);

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
//	if (!strcmp(str, "any")) {
		peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
//	} else {
//		host = gethostbyname(str);
//		if (!host || !host->h_addr_list[0]) { mess = "cannot resolve host ", mess1 = str; res = h_errno; goto err; }
//		// Write resolved IP address of a server to the address structure
//		memmove(&peeraddr.sin_addr.s_addr, host->h_addr_list[0], 4);
//	}

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

	str = strtok_r(0, " \t\r\n", &lasts);
	if (str) cheatcoin_pool_set_config(str);

	// Now, listen for a connection
	res = listen(sock, N_MINERS);    // "1" is the maximal length of the queue
	if (res) { mess = "cannot listen"; goto err; }

	for(;;) {
		// Accept a connection (the "accept" command waits for a connection with
		// no timeout limit...)
		fd = accept(sock, (struct sockaddr*) &peeraddr, &peeraddr_len);
		if (fd < 0) { mess = "cannot accept"; goto err; }
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbufsize, sizeof(int));
//		ioctl(fd, FIONBIO, (char*)&nonblock);

		t = cheatcoin_main_time();
		for (i = 0, count = 1, i0 = -1; i < g_nminers; ++i) {
			m = g_miners + i;
			if (m->state & MINER_FREE) { if (i0 < 0) i0 = i; }
			else if (m->state & MINER_ARCHIVE && t - m->main_time > N_CONFIRMATIONS) { if (i0 < 0) i0 = i; }
			else if (m->ip == peeraddr.sin_addr.s_addr && ++count > g_max_nminers_ip) goto closefd;
		}
		if (i0 >= 0) i = i0;
		if (i >= g_max_nminers) {
		closefd:
			close(fd);
		} else {
			m = g_miners + i;
			p = g_fds + i;
			p->fd = fd;
			p->events = POLLIN | POLLOUT;
			p->revents = 0;
			memset(m, 0, sizeof(struct miner));
			j = m->ip = peeraddr.sin_addr.s_addr;
			m->port = peeraddr.sin_port;
			if (i == g_nminers) g_nminers++;
			cheatcoin_info("Pool  : miner %d connected from %u.%u.%u.%u:%u", i,
					j & 0xff, j >> 8 & 0xff, j >> 16 & 0xff, j >> 24 & 0xff, ntohs(m->port));
		}
	}

	return 0;
err:
	cheatcoin_err("pool  : %s %s (error %d)", mess, mess1, res);
	return 0;
}

static int send_to_pool(struct cheatcoin_field *fld, int nfld) {
	struct cheatcoin_field f[CHEATCOIN_BLOCK_FIELDS];
	cheatcoin_hash_t h;
	struct miner *m = &g_local_miner;
	int i, res, todo = nfld * sizeof(struct cheatcoin_field), done = 0;
	if (g_socket < 0) {
		pthread_mutex_unlock(&g_pool_mutex);
		return -1;
	}
	memcpy(f, fld, todo);
	if (nfld == CHEATCOIN_BLOCK_FIELDS) {
		uint32_t crc;
		f[0].transport_header = 0;
		cheatcoin_hash(f, sizeof(struct cheatcoin_block), h);
		f[0].transport_header = HEADER_WORD;
		crc = crc_of_array((uint8_t *)f, sizeof(struct cheatcoin_block));
		f[0].transport_header |= (uint64_t)crc << 32;
	}
	for (i = 0; i < nfld; ++i)
		dfslib_encrypt_array(g_crypt, (uint32_t *)(f + i), DATA_SIZE, m->nfield_out++);
	while (todo) {
		struct pollfd p;
		p.fd = g_socket;
		p.events = POLLOUT;
		if (!poll(&p, 1, 1000)) continue;
		if (p.revents & (POLLHUP | POLLERR)) {
			pthread_mutex_unlock(&g_pool_mutex);
			return -1;
		}
		if (!(p.revents & POLLOUT)) continue;
		res = write(g_socket, (uint8_t *)f + done, todo);
		if (res <= 0) {
			pthread_mutex_unlock(&g_pool_mutex);
			return -1;
		}
		done += res, todo -= res;
	}
	pthread_mutex_unlock(&g_pool_mutex);
	if (nfld == CHEATCOIN_BLOCK_FIELDS) {
		cheatcoin_info("Sent  : %016llx%016llx%016llx%016llx t=%llx res=%d",
			h[3], h[2], h[1], h[0], fld[0].time, 0);
	}
	return 0;
}

/* послать блок в сеть через пул */
int cheatcoin_send_block_via_pool(struct cheatcoin_block *b) {
	if (g_socket < 0) return -1;
	pthread_mutex_lock(&g_pool_mutex);
	return send_to_pool(b->field, CHEATCOIN_BLOCK_FIELDS);
}


static void *miner_net_thread(void *arg) {
	struct cheatcoin_block *blk, b;
	struct cheatcoin_field data[2];
	cheatcoin_hash_t hash;
	const char *str = (const char *)arg, *s;
	char buf[0x100];
	const char *mess, *mess1 = "";
	struct sockaddr_in peeraddr;
	struct hostent *host;
	char *lasts;
	int res = 0, reuseaddr = 1, ndata, maxndata;
	int64_t pos;
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	cheatcoin_time_t t;
	struct miner *m = &g_local_miner;
	time_t t00, t0, tt;

	while (!g_cheatcoin_sync_on) sleep(1);

begin:
	ndata = 0;
	maxndata = sizeof(struct cheatcoin_field);
	t0 = t00 = 0;
	m->nfield_in = m->nfield_out = 0;

	if (g_miner_address) { if (cheatcoin_address2hash(g_miner_address, hash)) { mess = "incorrect miner address"; goto err; }}
	else if (cheatcoin_get_our_block(hash)) { mess = "can't create a block"; goto err; }
	pos = cheatcoin_get_block_pos(hash, &t);
	if (pos < 0) { mess = "can't find the block"; goto err; }
	blk = cheatcoin_storage_load(hash, t, pos, &b);
	if (!blk) { mess = "can't load the block"; goto err; }
	if (blk != &b) memcpy(&b, blk, sizeof(struct cheatcoin_block));

	pthread_mutex_lock(&g_pool_mutex);
	// Create a socket
	g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_socket == INVALID_SOCKET) { pthread_mutex_unlock(&g_pool_mutex); mess = "cannot create a socket"; goto err; }
	if (fcntl(g_socket, F_SETFD, FD_CLOEXEC) == -1) {
		cheatcoin_err("pool  : can't set FD_CLOEXEC flag on socket %d, %s\n", g_socket, strerror(errno));
	}

	// Fill in the address of server
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, str);
	s = strtok_r(buf, " \t\r\n:", &lasts);
	if (!s) { pthread_mutex_unlock(&g_pool_mutex); mess = "host is not given"; goto err; }
	if (!strcmp(s, "any")) {
		peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if (!inet_aton(s, &peeraddr.sin_addr)) {
		host = gethostbyname(s);
		if (!host || !host->h_addr_list[0]) { pthread_mutex_unlock(&g_pool_mutex); mess = "cannot resolve host ", mess1 = s; res = h_errno; goto err; }
		// Write resolved IP address of a server to the address structure
		memmove(&peeraddr.sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// Resolve port
	s = strtok_r(0, " \t\r\n:", &lasts);
	if (!s) { pthread_mutex_unlock(&g_pool_mutex); mess = "port is not given"; goto err; }
	peeraddr.sin_port = htons(atoi(s));

	// Set the "LINGER" timeout to zero, to close the listen socket
	// immediately at program termination.
	setsockopt(g_socket, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
	setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));

	// Now, connect to a pool
	res = connect(g_socket, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
	if (res) { pthread_mutex_unlock(&g_pool_mutex); mess = "cannot connect to the pool"; goto err; }

	if (send_to_pool(b.field, CHEATCOIN_BLOCK_FIELDS) < 0) { mess = "socket is closed"; goto err; }

	for(;;) {
		struct pollfd p;
		pthread_mutex_lock(&g_pool_mutex);
		if (g_socket < 0) { pthread_mutex_unlock(&g_pool_mutex); mess = "socket is closed"; goto err; }
		p.fd = g_socket;
		tt = time(0);
		p.events = POLLIN | (tt - t0 >= SEND_PERIOD && tt - t00 <= 64 ? POLLOUT : 0);
		if (!poll(&p, 1, 0)) { pthread_mutex_unlock(&g_pool_mutex); sleep(1); continue; }
		if (p.revents & POLLHUP) { pthread_mutex_unlock(&g_pool_mutex); mess = "socket hangup"; goto err; }
		if (p.revents & POLLERR) { pthread_mutex_unlock(&g_pool_mutex); mess = "socket error"; goto err; }
		if (p.revents & POLLIN) {
			res = read(g_socket, (uint8_t *)data + ndata, maxndata - ndata);
			if (res < 0) { pthread_mutex_unlock(&g_pool_mutex); mess = "read error on socket"; goto err; }
			ndata += res;
			if (ndata == maxndata) {
				struct cheatcoin_field *last = data + (ndata / sizeof(struct cheatcoin_field) - 1);
				dfslib_uncrypt_array(g_crypt, (uint32_t *)last->data, DATA_SIZE, m->nfield_in++);
				if (!memcmp(last->data, hash, sizeof(cheatcoin_hashlow_t))) {
					cheatcoin_set_balance(hash, last->amount);
					g_cheatcoin_last_received = tt;
					ndata = 0;
					maxndata = sizeof(struct cheatcoin_field);
				} else if (maxndata == 2 * sizeof(struct cheatcoin_field)){
					uint64_t ntask = g_cheatcoin_pool_ntask + 1;
					struct cheatcoin_pool_task *task = &g_cheatcoin_pool_task[ntask & 1];
					task->main_time = cheatcoin_main_time();
					cheatcoin_hash_set_state(task->ctx, data[0].data,
							sizeof(struct cheatcoin_block) - 2 * sizeof(struct cheatcoin_field));
					cheatcoin_hash_update(task->ctx, data[1].data, sizeof(struct cheatcoin_field));
					cheatcoin_hash_update(task->ctx, hash, sizeof(cheatcoin_hashlow_t));
					cheatcoin_generate_random_array(task->nonce.data, sizeof(cheatcoin_hash_t));
					memcpy(task->nonce.data, hash, sizeof(cheatcoin_hashlow_t));
					memcpy(task->lastfield.data, task->nonce.data, sizeof(cheatcoin_hash_t));
					cheatcoin_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);
					g_cheatcoin_pool_ntask = ntask;
					t00 = time(0);
					cheatcoin_info("Task  : t=%llx N=%llu", task->main_time << 16 | 0xffff, ntask);
					ndata = 0;
					maxndata = sizeof(struct cheatcoin_field);
				} else {
					maxndata = 2 * sizeof(struct cheatcoin_field);
				}
			}
		}
		if (p.revents & POLLOUT) {
			uint64_t ntask = g_cheatcoin_pool_ntask;
			struct cheatcoin_pool_task *task = &g_cheatcoin_pool_task[ntask & 1];
			uint64_t *h = task->minhash.data;
			t0 = time(0);
			res = send_to_pool(&task->lastfield, 1);
			cheatcoin_info("Share : %016llx%016llx%016llx%016llx t=%llx res=%d",
				h[3], h[2], h[1], h[0], task->main_time << 16 | 0xffff, res);
			if (res) { mess = "write error on socket"; goto err; }
		} else pthread_mutex_unlock(&g_pool_mutex);
	}

	return 0;
err:
	cheatcoin_err("Miner : %s %s (error %d)", mess, mess1, res);
	pthread_mutex_lock(&g_pool_mutex);
	if (g_socket != INVALID_SOCKET) { close(g_socket); g_socket = INVALID_SOCKET; }
	pthread_mutex_unlock(&g_pool_mutex);
	sleep(5);
	goto begin;
}

static int crypt_start(void) {
	struct dfslib_string str;
	uint32_t sector0[128];
	int i;
	g_crypt = malloc(sizeof(struct dfslib_crypt));
	if (!g_crypt) return -1;
	dfslib_crypt_set_password(g_crypt, dfslib_utf8_string(&str, MINERS_PWD, strlen(MINERS_PWD)));
	for (i = 0; i < 128; ++i) sector0[i] = SECTOR0_BASE + i * SECTOR0_OFFSET;
	for (i = 0; i < 128; ++i) {
		dfslib_crypt_set_sector0(g_crypt, sector0);
		dfslib_encrypt_sector(g_crypt, sector0, SECTOR0_BASE + i * SECTOR0_OFFSET);
	}
	return 0;
}

static void *mining_thread(void *arg) {
	struct cheatcoin_pool_task *task;
	cheatcoin_hash_t hash;
	struct cheatcoin_field last;
	int nthread = (int)(uintptr_t)arg;
	uint64_t ntask, oldntask = 0;
	uint64_t nonce;
	while (!g_cheatcoin_sync_on && !g_stop_mining) sleep(1);
	while (!g_stop_mining) {
		ntask = g_cheatcoin_pool_ntask;
		task = &g_cheatcoin_pool_task[ntask & 1];
		if (!ntask) { sleep(1); continue; }
		if (ntask != oldntask) {
			oldntask = ntask;
			memcpy(last.data, task->nonce.data, sizeof(cheatcoin_hash_t));
			nonce = last.amount + nthread;
		}
		last.amount = cheatcoin_hash_final_multi(task->ctx, &nonce, 4096, g_cheatcoin_mining_threads, hash);
		g_cheatcoin_extstats.nhashes += 4096;
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
	if ((n_mining_threads > 0 || g_cheatcoin_pool) && g_stop_general_mining) {
		cheatcoin_mess("Starting general mining thread...");
		g_stop_general_mining = 0;
		pthread_create(&th, 0, general_mining_thread, 0);
		pthread_detach(th);
	}
	if (n_mining_threads < 0) n_mining_threads = ~n_mining_threads;
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
	cheatcoin_get_our_block(g_local_miner.id.data);
	g_local_miner.state = (g_cheatcoin_mining_threads ? 0 : MINER_ARCHIVE);
	return 0;
}

int cheatcoin_pool_start(int pool_on, const char *pool_arg, const char *miner_address) {
	pthread_t th;
	int i, res;
	g_cheatcoin_pool = pool_on;
	g_miner_address = miner_address;
	for (i = 0; i < 2; ++i) {
		g_cheatcoin_pool_task[i].ctx0 = malloc(cheatcoin_hash_ctx_size());
		g_cheatcoin_pool_task[i].ctx = malloc(cheatcoin_hash_ctx_size());
		if (!g_cheatcoin_pool_task[i].ctx0 || !g_cheatcoin_pool_task[i].ctx) return -1;
	}
	if (!pool_on && !pool_arg) return 0;
	if (crypt_start()) return -1;
	memset(&g_local_miner, 0, sizeof(struct miner));
	memset(&g_fund_miner, 0, sizeof(struct miner));
	if (!pool_on) {
		res = pthread_create(&th, 0, miner_net_thread, (void *)pool_arg);
		if (res) return -1;
		pthread_detach(th);
		return 0;
	}
	g_miners = malloc(N_MINERS * sizeof(struct miner));
	g_fds = malloc(N_MINERS * sizeof(struct pollfd));
	if (!g_miners || !g_fds) return -1;
	res = pthread_create(&th, 0, pool_net_thread, (void *)pool_arg);
	if (res) return -1;
	pthread_detach(th);
	res = pthread_create(&th, 0, pool_main_thread, 0);
	if (res) return -1;
	pthread_detach(th);
	res = pthread_create(&th, 0, pool_block_thread, 0);
	if (res) return -1;
	pthread_detach(th);
	return 0;
}

static int print_miner(FILE *out, int n, struct miner *m) {
	double sum = m->prev_diff;
	int count = m->prev_diff_count;
	char buf[32], buf2[64];
	uint32_t i = m->ip;
	int j;
	for (j = 0; j < N_CONFIRMATIONS; ++j) if (m->maxdiff[j] > 0) { sum += m->maxdiff[j]; count++; }
	sprintf(buf, "%u.%u.%u.%u:%u", i & 0xff, i >> 8 & 0xff, i >> 16 & 0xff, i >> 24 & 0xff, ntohs(m->port));
	sprintf(buf2, "%llu/%llu", (unsigned long long)m->nfield_in * sizeof(struct cheatcoin_field),
			(unsigned long long)m->nfield_out * sizeof(struct cheatcoin_field));
	fprintf(out, "%3d. %s  %s  %-21s  %-16s  %lf\n", n, cheatcoin_hash2address(m->id.data),
		(m->state & MINER_FREE ? "free   " : (m->state & MINER_ARCHIVE ? "archive" :
		(m->state & MINER_ADDRESS ? "active " : "badaddr"))), buf, buf2, diff2pay(sum, count));
	return m->state & (MINER_FREE | MINER_ARCHIVE) ? 0 : 1;
}

int cheatcoin_print_miners(FILE *out) {
	int i, res;
	fprintf(out, "List of miners:\n"
" NN  Address for payment to            Status   IP and port            in/out bytes      nopaid shares\n"
"------------------------------------------------------------------------------------------------------\n");
	res = print_miner(out, -1, &g_local_miner);
	for (i = 0; i < g_nminers; ++i)
		res += print_miner(out, i, g_miners + i);
	fprintf(out,
"------------------------------------------------------------------------------------------------------\n"
		"Total %d active miners.\n", res);
	return res;
}
