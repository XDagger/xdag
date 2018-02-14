/* pool and miner, T13.744-T13.836 $ DVS: time $ */

#ifndef CHEATCOIN_POOL_H
#define CHEATCOIN_POOL_H

#include <stdio.h>
#include "block.h"
#include "hash.h"

#define CHEATCOIN_POOL_N_CONFIRMATIONS	16

struct cheatcoin_pool_task {
	struct cheatcoin_field task[2], lastfield, minhash, nonce;
	cheatcoin_time_t main_time;
	void *ctx0, *ctx;
};

/* initialization of the pool (pool_on = 1) or connection of the miner to the pool (pool_on = 0; pool_arg - parameters of the pool ip: port [: CFG]; miner_addr - address of the miner, */
extern int cheatcoin_pool_start(int pool_on, const char *pool_arg, const char *miner_address);

/* changes number of threads of mining */
extern int cheatcoin_mining_start(int n_mining_threads);

/* gets pool parameters as a string, 0 - if the pool is disabled */
extern char *cheatcoin_pool_get_config(char *buf);

/* sets pool parameters */
extern int cheatcoin_pool_set_config(const char *str);

/* send block to network via pool */
extern int cheatcoin_send_block_via_pool(struct cheatcoin_block *b);

/* output to the file a list of miners */
extern int cheatcoin_print_miners(FILE *out);

extern struct cheatcoin_pool_task g_cheatcoin_pool_task[2];
extern uint64_t g_cheatcoin_pool_ntask;
extern cheatcoin_hash_t g_cheatcoin_mined_hashes[CHEATCOIN_POOL_N_CONFIRMATIONS],
						g_cheatcoin_mined_nonce[CHEATCOIN_POOL_N_CONFIRMATIONS];
/* number of own mining flows */
extern int g_cheatcoin_mining_threads;
/* a pointer to the mutex blocking the optimal ball */
extern void *g_ptr_share_mutex;

#endif
