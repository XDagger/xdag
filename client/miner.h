/* pool and miner logic o_O, T13.744-T13.836 $DVS:time$ */

#ifndef XDAG_MINER_H
#define XDAG_MINER_H

#include <stdio.h>
#include "block.h"
#include "hash.h"
#include "mining_common.h"

/* a number of mining threads */
extern int g_xdag_mining_threads;

/* changes the number of mining threads */
extern int xdag_mining_start(int n_mining_threads);

/* initialization of connection the miner to pool */
extern int xdag_initialize_miner(const char *pool_address);

extern void *miner_net_thread(void *arg);

/* send block to network via pool */
extern int xdag_send_block_via_pool(struct xdag_block *block);

#endif
