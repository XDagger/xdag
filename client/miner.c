/* пул и майнер, T13.744-T13.895 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "system.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "address.h"
#include "block.h"
#include "init.h"
#include "miner.h"
#include "storage.h"
#include "wallet.h"
#include "sync.h"
#include "transport.h"
#include "utils/log.h"
#include "commands.h"

#define MINERS_PWD             "minersgonnamine"
#define SECTOR0_BASE           0x1947f3acu
#define SECTOR0_OFFSET         0x82e9d1b5u
#define HEADER_WORD            0x3fca9e2bu
#define SEND_PERIOD            10                                  /* share period of sending shares */

struct miner {
	struct xdag_field id;
	uint64_t nfield_in;
	uint64_t nfield_out;
};

static struct miner g_local_miner;

/* a number of mining threads */
int g_xdag_mining_threads = 0;

static int g_socket = -1, g_stop_mining = 1, g_stop_general_mining = 1;

/* initialization of connection the miner to pool */
extern int xdag_initialize_miner(const char *pool_address)
{
	pthread_t th;

	memset(&g_local_miner, 0, sizeof(struct miner));
	xdag_get_our_block(g_local_miner.id.data);

	int res = pthread_create(&th, 0, miner_net_thread, (void*)pool_address);
	if(res) return -1;

	pthread_detach(th);

	return 0;
}

static int send_to_pool(struct xdag_field *fld, int nfld)
{
	struct xdag_field f[XDAG_BLOCK_FIELDS];
	xdag_hash_t h;
	struct miner *m = &g_local_miner;
	int i, res, todo = nfld * sizeof(struct xdag_field), done = 0;

	if(g_socket < 0) {
		pthread_mutex_unlock(&g_pool_mutex);
		return -1;
	}

	memcpy(f, fld, todo);

	if(nfld == XDAG_BLOCK_FIELDS) {
		f[0].transport_header = 0;

		xdag_hash(f, sizeof(struct xdag_block), h);

		f[0].transport_header = HEADER_WORD;

		uint32_t crc = crc_of_array((uint8_t*)f, sizeof(struct xdag_block));

		f[0].transport_header |= (uint64_t)crc << 32;
	}

	for(i = 0; i < nfld; ++i) {
		dfslib_encrypt_array(g_crypt, (uint32_t*)(f + i), DATA_SIZE, m->nfield_out++);
	}

	while(todo) {
		struct pollfd p;

		p.fd = g_socket;
		p.events = POLLOUT;

		if(!poll(&p, 1, 1000)) continue;

		if(p.revents & (POLLHUP | POLLERR)) {
			pthread_mutex_unlock(&g_pool_mutex);
			return -1;
		}

		if(!(p.revents & POLLOUT)) continue;

		res = write(g_socket, (uint8_t*)f + done, todo);
		if(res <= 0) {
			pthread_mutex_unlock(&g_pool_mutex);
			return -1;
		}

		done += res, todo -= res;
	}

	pthread_mutex_unlock(&g_pool_mutex);

	if(nfld == XDAG_BLOCK_FIELDS) {
		xdag_info("Sent  : %016llx%016llx%016llx%016llx t=%llx res=%d",
			h[3], h[2], h[1], h[0], fld[0].time, 0);
	}

	return 0;
}

void *miner_net_thread(void *arg)
{
	struct xdag_block b;
	struct xdag_field data[2];
	xdag_hash_t hash;
	const char *str = (const char*)arg;
	char buf[0x100];
	const char *mess, *mess1 = "";
	struct sockaddr_in peeraddr;
	char *lasts;
	int res = 0, reuseaddr = 1;
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
	xdag_time_t t;
	struct miner *m = &g_local_miner;
	time_t t00, t0, tt;
	int ndata, maxndata;

	while(!g_xdag_sync_on) {
		sleep(1);
	}

begin:
	ndata = 0;
	maxndata = sizeof(struct xdag_field);
	t0 = t00 = 0;
	m->nfield_in = m->nfield_out = 0;

	if(g_miner_address) {
		if(xdag_address2hash(g_miner_address, hash)) {
			mess = "incorrect miner address";
			goto err;
		}
	} else if(xdag_get_our_block(hash)) {
		mess = "can't create a block";
		goto err;
	}

	const int64_t pos = xdag_get_block_pos(hash, &t);

	if(pos < 0) {
		mess = "can't find the block";
		goto err;
	}

	struct xdag_block *blk = xdag_storage_load(hash, t, pos, &b);
	if(!blk) {
		mess = "can't load the block";
		goto err;
	}
	if(blk != &b) memcpy(&b, blk, sizeof(struct xdag_block));

	pthread_mutex_lock(&g_pool_mutex);
	// Create a socket
	g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(g_socket == INVALID_SOCKET) {
		pthread_mutex_unlock(&g_pool_mutex);
		mess = "cannot create a socket";
		goto err;
	}
	if(fcntl(g_socket, F_SETFD, FD_CLOEXEC) == -1) {
		xdag_err("pool  : can't set FD_CLOEXEC flag on socket %d, %s\n", g_socket, strerror(errno));
	}

	// Fill in the address of server
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, str);
	const char *s = strtok_r(buf, " \t\r\n:", &lasts);
	if(!s) {
		pthread_mutex_unlock(&g_pool_mutex);
		mess = "host is not given";
		goto err;
	}
	if(!strcmp(s, "any")) {
		peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if(!inet_aton(s, &peeraddr.sin_addr)) {
		struct hostent *host = gethostbyname(s);
		if(!host || !host->h_addr_list[0]) {
			pthread_mutex_unlock(&g_pool_mutex);
			mess = "cannot resolve host ";
			mess1 = s;
			res = h_errno;
			goto err;
		}
		// Write resolved IP address of a server to the address structure
		memmove(&peeraddr.sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// Resolve port
	s = strtok_r(0, " \t\r\n:", &lasts);
	if(!s) {
		pthread_mutex_unlock(&g_pool_mutex);
		mess = "port is not given";
		goto err;
	}
	peeraddr.sin_port = htons(atoi(s));

	// Set the "LINGER" timeout to zero, to close the listen socket
	// immediately at program termination.
	setsockopt(g_socket, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));
	setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(int));

	// Now, connect to a pool
	res = connect(g_socket, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
	if(res) {
		pthread_mutex_unlock(&g_pool_mutex);
		mess = "cannot connect to the pool";
		goto err;
	}

	if(send_to_pool(b.field, XDAG_BLOCK_FIELDS) < 0) {
		mess = "socket is closed";
		goto err;
	}

	for(;;) {
		struct pollfd p;

		pthread_mutex_lock(&g_pool_mutex);

		if(g_socket < 0) {
			pthread_mutex_unlock(&g_pool_mutex);
			mess = "socket is closed";
			goto err;
		}

		p.fd = g_socket;
		tt = time(0);
		p.events = POLLIN | (tt - t0 >= SEND_PERIOD && tt - t00 <= 64 ? POLLOUT : 0);

		if(!poll(&p, 1, 0)) {
			pthread_mutex_unlock(&g_pool_mutex);
			sleep(1);
			continue;
		}

		if(p.revents & POLLHUP) {
			pthread_mutex_unlock(&g_pool_mutex);
			mess = "socket hangup";
			goto err;
		}

		if(p.revents & POLLERR) {
			pthread_mutex_unlock(&g_pool_mutex);
			mess = "socket error";
			goto err;
		}

		if(p.revents & POLLIN) {
			res = read(g_socket, (uint8_t*)data + ndata, maxndata - ndata);
			if(res < 0) {
				pthread_mutex_unlock(&g_pool_mutex); mess = "read error on socket"; goto err;
			}
			ndata += res;
			if(ndata == maxndata) {
				struct xdag_field *last = data + (ndata / sizeof(struct xdag_field) - 1);

				dfslib_uncrypt_array(g_crypt, (uint32_t*)last->data, DATA_SIZE, m->nfield_in++);

				if(!memcmp(last->data, hash, sizeof(xdag_hashlow_t))) {
					xdag_set_balance(hash, last->amount);

					g_xdag_last_received = tt;
					ndata = 0;

					maxndata = sizeof(struct xdag_field);
				} else if(maxndata == 2 * sizeof(struct xdag_field)) {
					const uint64_t task_index = g_xdag_pool_task_index + 1;
					struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];

					task->task_time = xdag_main_time();
					xdag_hash_set_state(task->ctx, data[0].data,
						sizeof(struct xdag_block) - 2 * sizeof(struct xdag_field));
					xdag_hash_update(task->ctx, data[1].data, sizeof(struct xdag_field));
					xdag_hash_update(task->ctx, hash, sizeof(xdag_hashlow_t));

					xdag_generate_random_array(task->nonce.data, sizeof(xdag_hash_t));

					memcpy(task->nonce.data, hash, sizeof(xdag_hashlow_t));
					memcpy(task->lastfield.data, task->nonce.data, sizeof(xdag_hash_t));

					xdag_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);

					g_xdag_pool_task_index = task_index;
					t00 = time(0);

					xdag_info("Task  : t=%llx N=%llu", task->task_time << 16 | 0xffff, task_index);

					ndata = 0;
					maxndata = sizeof(struct xdag_field);
				} else {
					maxndata = 2 * sizeof(struct xdag_field);
				}
			}
		}

		if(p.revents & POLLOUT) {
			const uint64_t task_index = g_xdag_pool_task_index;
			struct xdag_pool_task *task = &g_xdag_pool_task[task_index & 1];
			uint64_t *h = task->minhash.data;

			t0 = time(0);
			res = send_to_pool(&task->lastfield, 1);

			xdag_info("Share : %016llx%016llx%016llx%016llx t=%llx res=%d",
				h[3], h[2], h[1], h[0], task->task_time << 16 | 0xffff, res);

			if(res) {
				mess = "write error on socket"; goto err;
			}
		} else {
			pthread_mutex_unlock(&g_pool_mutex);
		}
	}

	return 0;

err:
	xdag_err("Miner : %s %s (error %d)", mess, mess1, res);

	pthread_mutex_lock(&g_pool_mutex);

	if(g_socket != INVALID_SOCKET) {
		close(g_socket); g_socket = INVALID_SOCKET;
	}

	pthread_mutex_unlock(&g_pool_mutex);

	sleep(5);

	goto begin;
}

static void *mining_thread(void *arg)
{
	xdag_hash_t hash;
	struct xdag_field last;
	const int nthread = (int)(uintptr_t)arg;
	uint64_t oldntask = 0;
	uint64_t nonce;

	while(!g_xdag_sync_on && !g_stop_mining) {
		sleep(1);
	}

	while(!g_stop_mining) {
		const uint64_t ntask = g_xdag_pool_task_index;
		struct xdag_pool_task *task = &g_xdag_pool_task[ntask & 1];

		if(!ntask) {
			sleep(1);
			continue;
		}

		if(ntask != oldntask) {
			oldntask = ntask;
			memcpy(last.data, task->nonce.data, sizeof(xdag_hash_t));
			nonce = last.amount + nthread;
		}

		last.amount = xdag_hash_final_multi(task->ctx, &nonce, 4096, g_xdag_mining_threads, hash);
		g_xdag_extstats.nhashes += 4096;

		xdag_set_min_share(task, last.data, hash);
	}

	return 0;
}

static void *general_mining_thread(void *arg)
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

/* changes the number of mining threads */
int xdag_mining_start(int n_mining_threads)
{
	pthread_t th;

	if((n_mining_threads > 0 || g_xdag_pool) && g_stop_general_mining) {
		xdag_mess("Starting general mining thread...");

		g_stop_general_mining = 0;

		pthread_create(&th, 0, general_mining_thread, 0);
		pthread_detach(th);
	}

	if(n_mining_threads < 0) {
		n_mining_threads = ~n_mining_threads;
	}

	if(n_mining_threads == g_xdag_mining_threads) {

	} else if(!n_mining_threads) {
		g_stop_mining = 1;
		if(!g_xdag_pool) g_stop_general_mining = 1;
		g_xdag_mining_threads = 0;
	} else if(!g_xdag_mining_threads) {
		g_stop_mining = 0;
	} else if(g_xdag_mining_threads > n_mining_threads) {
		g_stop_mining = 1;
		sleep(5);
		g_stop_mining = 0;
		g_xdag_mining_threads = 0;
	}

	while(g_xdag_mining_threads < n_mining_threads) {
		pthread_create(&th, 0, mining_thread, (void*)(uintptr_t)g_xdag_mining_threads);
		pthread_detach(th);
		g_xdag_mining_threads++;
	}

	return 0;
}

/* send block to network via pool */
int xdag_send_block_via_pool(struct xdag_block *b)
{
	if(g_socket < 0) return -1;

	pthread_mutex_lock(&g_pool_mutex);

	return send_to_pool(b->field, XDAG_BLOCK_FIELDS);
}
