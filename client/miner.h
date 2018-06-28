/* pool and miner logic o_O, T13.744-T13.836 $DVS:time$ */

#ifndef XDAG_MINER_H
#define XDAG_MINER_H

#include <stdio.h>
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* a number of mining threads */
extern int g_xdag_mining_threads;

/* changes the number of mining threads */
extern int xdag_mining_start(int n_mining_threads);

/* initialization of connection the miner to pool */
extern int xdag_initialize_miner(const char *pool_address);

/* create new wallet address */
extern int xdag_miner_new_address(xdag_hash_t hash);

extern void *miner_refresh_thread(void *arg);

extern void *miner_net_thread(void *arg);

/* send block to network via pool */
extern int xdag_send_block_via_pool(struct xdag_block *block);

#ifdef __cplusplus
};
#endif
		
#endif
