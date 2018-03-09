/* pool and miner logic o_O, T13.744-T13.836 $DVS:time$ */

#ifndef XDAG_POOL_H
#define XDAG_POOL_H

#include <stdio.h>
#include "block.h"
#include "hash.h"

#define XDAG_POOL_N_CONFIRMATIONS  16

struct xdag_pool_task {
	struct xdag_field task[2], lastfield, minhash, nonce;
	xdag_time_t task_time;
	void *ctx0, *ctx;
};

/* initialization of the pool (pool_on = 1) or connecting the miner to pool (pool_on = 0; pool_arg - pool parameters ip:port[:CFG];
   miner_addr - address of the miner, if specified */
extern int xdag_pool_start(int pool_on, const char *pool_arg, const char *miner_address);

/* changes the number of mining threads */
extern int xdag_mining_start(int n_mining_threads);

/* gets pool parameters as a string, 0 - if the pool is disabled */
extern char *xdag_pool_get_config(char *buf);

/* sets pool parameters */
extern int xdag_pool_set_config(const char *pool_config);

/* send block to network via pool */
extern int xdag_send_block_via_pool(struct xdag_block *b);

/* output to the file a list of miners */
extern int xdag_print_miners(FILE *out);

extern struct xdag_pool_task g_xdag_pool_task[2];
extern uint64_t g_xdag_pool_task_index;
extern xdag_hash_t g_xdag_mined_hashes[XDAG_POOL_N_CONFIRMATIONS],
						g_xdag_mined_nonce[XDAG_POOL_N_CONFIRMATIONS];
/* a number of mining threads */
extern int g_xdag_mining_threads;
/* poiter to mutex for optimal share  */
extern void *g_ptr_share_mutex;

#endif
