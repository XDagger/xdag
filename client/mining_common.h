#ifndef XDAG_MINING_COMMON_H
#define XDAG_MINING_COMMON_H

#include <pthread.h>
#include "block.h"

#define MAX_MINERS_COUNT               4096
#define XDAG_POOL_CONFIRMATIONS_COUNT  16
#define DATA_SIZE                      (sizeof(struct xdag_field) / sizeof(uint32_t))
#define CONFIRMATIONS_COUNT            XDAG_POOL_CONFIRMATIONS_COUNT   /*16*/

enum miner_state {
	MINER_BLOCK = 1,
	MINER_ARCHIVE = 2,
	MINER_FREE = 4,
	MINER_BALANCE = 8,
	MINER_ADDRESS = 0x10,
};

struct miner {
	struct xdag_field id;
	xdag_time_t task_time;
	double prev_diff;
	uint32_t prev_diff_count;
	double maxdiff[CONFIRMATIONS_COUNT];
	uint32_t data[DATA_SIZE];
	uint64_t nfield_in;
	uint64_t nfield_out;
	uint64_t task_index;
	struct xdag_block *block;
	uint32_t ip;
	uint16_t port;
	uint16_t state;
	uint8_t data_size;
	uint8_t block_size;
	//uint32_t shares_count;
};

struct xdag_pool_task {
	struct xdag_field task[2], lastfield, minhash, nonce;
	xdag_time_t task_time;
	void *ctx0, *ctx;
};

extern struct xdag_pool_task g_xdag_pool_task[2];
extern uint64_t g_xdag_pool_task_index;

/* poiter to mutex for optimal share  */
extern void *g_ptr_share_mutex;

/* 1 - program works as a pool */
extern int g_xdag_pool;

extern const char *g_miner_address;

extern struct miner g_local_miner;
extern struct miner g_fund_miner;
extern struct miner *g_miners;
extern struct pollfd *g_fds;

extern pthread_mutex_t g_pool_mutex;
extern pthread_mutex_t g_share_mutex;

extern struct dfslib_crypt *g_crypt;

/* initialization of the pool (pool_on = 1) or connecting the miner to pool (pool_on = 0; pool_arg - pool parameters ip:port[:CFG];
miner_addr - address of the miner, if specified */
extern int xdag_initialize_mining(int pool_on, const char *pool_arg, const char *miner_address);

#endif