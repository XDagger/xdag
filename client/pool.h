/* pool logic */

#ifndef XDAG_POOL_H
#define XDAG_POOL_H

#include <stdio.h>
#include "block.h"
#include "hash.h"
#include "mining_common.h"

#define MAX_MINERS_COUNT               4096
#define XDAG_POOL_CONFIRMATIONS_COUNT  16
#define CONFIRMATIONS_COUNT            XDAG_POOL_CONFIRMATIONS_COUNT   /*16*/

extern xdag_hash_t g_xdag_mined_hashes[XDAG_POOL_CONFIRMATIONS_COUNT];
extern xdag_hash_t g_xdag_mined_nonce[XDAG_POOL_CONFIRMATIONS_COUNT];

/* initialization of the pool */
extern int xdag_initialize_pool(const char *pool_arg);

/* gets pool parameters as a string, 0 - if the pool is disabled */
extern char *xdag_pool_get_config(char *buf);

/* sets pool parameters */
extern int xdag_pool_set_config(const char *pool_config);

/* output to the file a list of miners */
extern int xdag_print_miners(FILE *out);

extern void *pool_net_thread(void *arg);
extern void *pool_main_thread(void *arg);
extern void *pool_block_thread(void *arg);

#endif
