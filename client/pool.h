/* pool logic */

#ifndef XDAG_POOL_H
#define XDAG_POOL_H

#include <stdio.h>
#include "block.h"
#include "hash.h"
#include "mining_common.h"

extern xdag_hash_t g_xdag_mined_hashes[XDAG_POOL_N_CONFIRMATIONS],
	g_xdag_mined_nonce[XDAG_POOL_N_CONFIRMATIONS];

/* gets pool parameters as a string, 0 - if the pool is disabled */
extern char *xdag_pool_get_config(char *buf);

/* sets pool parameters */
extern int xdag_pool_set_config(const char *pool_config);

/* send block to network via pool */
extern int xdag_send_block_via_pool(struct xdag_block *b);

/* output to the file a list of miners */
extern int xdag_print_miners(FILE *out);

extern void *pool_net_thread(void *arg);
extern void *pool_main_thread(void *arg);
extern void *pool_block_thread(void *arg);

#endif
