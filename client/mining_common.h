#ifndef XDAG_MINING_COMMON_H
#define XDAG_MINING_COMMON_H

#include <pthread.h>
#include "block.h"
#if defined(_WIN32) || defined(_WIN64)
#if defined(_WIN64)
#define poll WSAPoll
#else
#define poll(a, b, c) ((a)->revents = (a)->events, (b))
#endif
#else
#include <poll.h>
#endif

#define DATA_SIZE          (sizeof(struct xdag_field) / sizeof(uint32_t))
#define BLOCK_HEADER_WORD  0x3fca9e2bu

struct xdag_pool_task {
	struct xdag_field task[2], lastfield, minhash, nonce;
	xdag_time_t task_time;
	void *ctx0, *ctx;
};

extern struct xdag_pool_task g_xdag_pool_task[2];
extern uint64_t g_xdag_pool_task_index; /* global variables are instantiated with 0 */

/* poiter to mutex for optimal share  */
extern void *g_ptr_share_mutex;

/* 1 - program works as a pool */
extern int g_xdag_pool;

extern const char *g_miner_address;

extern pthread_mutex_t g_share_mutex;

extern struct dfslib_crypt *g_crypt;

/* initialization of the pool (g_xdag_pool = 1) or connecting the miner to pool (g_xdag_pool = 0; pool_arg - pool parameters ip:port[:CFG];
miner_addr - address of the miner, if specified */
extern int xdag_initialize_mining(const char *pool_arg, const char *miner_address);

//function sets minimal share for the task
extern void xdag_set_min_share(struct xdag_pool_task *task, xdag_hash_t last, xdag_hash_t hash);

#endif
