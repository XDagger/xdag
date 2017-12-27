/* пул и майнер, T13.744-T13.000 $DVS:time$ */

#ifndef CHEATCOIN_POOL_H
#define CHEATCOIN_POOL_H

#include "block.h"
#include "hash.h"

struct cheatcoin_pool_task {
	struct cheatcoin_field task[2], lastfield, minhash, nonce;
	cheatcoin_time_t main_time;
	void *ctx0, *ctx;
};

extern int cheatcoin_pool_start(int pool_on, const char *pool_arg);

extern int cheatcoin_mining_start(int n_mining_threads);

extern struct cheatcoin_pool_task g_cheatcoin_pool_task[2];
extern uint64_t g_cheatcoin_pool_ntask;
/* число собственных потоков майнинга */
extern int g_cheatcoin_mining_threads;

#endif
