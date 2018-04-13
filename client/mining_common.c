#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "mining_common.h"
#include "miner.h"
#include "pool.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"

#define MINERS_PWD             "minersgonnamine"
#define SECTOR0_BASE           0x1947f3acu
#define SECTOR0_OFFSET         0x82e9d1b5u

/* 1 - program works as a pool */
int g_xdag_pool = 0;

struct xdag_pool_task g_xdag_pool_task[2];
uint64_t g_xdag_pool_task_index;

const char *g_miner_address;
struct miner g_local_miner;
struct miner g_fund_miner;
struct miner *g_miners;
struct pollfd *g_fds;

pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_share_mutex = PTHREAD_MUTEX_INITIALIZER;

struct dfslib_crypt *g_crypt;

/* poiter to mutex for optimal share  */
void *g_ptr_share_mutex = &g_share_mutex;

static int crypt_start(void)
{
	struct dfslib_string str;
	uint32_t sector0[128];
	int i;

	g_crypt = malloc(sizeof(struct dfslib_crypt));
	if(!g_crypt) return -1;
	dfslib_crypt_set_password(g_crypt, dfslib_utf8_string(&str, MINERS_PWD, strlen(MINERS_PWD)));

	for(i = 0; i < 128; ++i) {
		sector0[i] = SECTOR0_BASE + i * SECTOR0_OFFSET;
	}

	for(i = 0; i < 128; ++i) {
		dfslib_crypt_set_sector0(g_crypt, sector0);
		dfslib_encrypt_sector(g_crypt, sector0, SECTOR0_BASE + i * SECTOR0_OFFSET);
	}

	return 0;
}

/* initialization of the pool (pool_on = 1) or connecting the miner to pool (pool_on = 0; pool_arg - pool parameters ip:port[:CFG];
miner_addr - address of the miner, if specified */
int xdag_initialize_mining(int pool_on, const char *pool_arg, const char *miner_address)
{
	pthread_t th;
	int res;

	g_xdag_pool = pool_on;
	g_miner_address = miner_address;

	for(int i = 0; i < 2; ++i) {
		g_xdag_pool_task[i].ctx0 = malloc(xdag_hash_ctx_size());
		g_xdag_pool_task[i].ctx = malloc(xdag_hash_ctx_size());

		if(!g_xdag_pool_task[i].ctx0 || !g_xdag_pool_task[i].ctx) {
			return -1;
		}
	}

	if(!pool_on && !pool_arg) return 0;

	if(crypt_start()) return -1;

	memset(&g_local_miner, 0, sizeof(struct miner));
	memset(&g_fund_miner, 0, sizeof(struct miner));

	if(!pool_on) {
		res = pthread_create(&th, 0, miner_net_thread, (void*)pool_arg);
		if(res) return -1;

		pthread_detach(th);

		return 0;
	}

	g_miners = malloc(MAX_MINERS_COUNT * sizeof(struct miner));
	g_fds = malloc(MAX_MINERS_COUNT * sizeof(struct pollfd));
	if(!g_miners || !g_fds) return -1;

	res = pthread_create(&th, 0, pool_net_thread, (void*)pool_arg);
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

//function sets minimal share for the task
static void xdag_set_min_share(struct xdag_pool_task *task, xdag_hash_t last, xdag_hash_t hash)
{
	if(xdag_cmphash(hash, task->minhash.data) < 0) {
		pthread_mutex_lock(&g_share_mutex);

		if(xdag_cmphash(hash, task->minhash.data) < 0) {
			memcpy(task->minhash.data, hash, sizeof(xdag_hash_t));
			memcpy(task->lastfield.data, last, sizeof(xdag_hash_t));
		}

		pthread_mutex_unlock(&g_share_mutex);
	}
}