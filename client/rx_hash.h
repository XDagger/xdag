//
// copy from mac on 2020/7/29.
//

#ifndef XDAG_RX_HASH_H
#define XDAG_RX_HASH_H

#include <stdint.h>
#include <stdlib.h>
#include <randomx.h>
#include "hash.h"
#include "time.h"
#include "global.h"
#include "block.h"

#define SEEDHASH_EPOCH_BLOCKS   4096
#define SEEDHASH_EPOCH_LAG      128 // lag time frames

#define SEEDHASH_EPOCH_TESTNET_BLOCKS	64
#define SEEDHASH_EPOCH_TESTNET_LAG		32

#define RANDOMX_FORK_HEIGHT           1339392 // fork seed height, it's time frame + SEEDHASH_EPOCH_LAG = fork time frame
#define RANDOMX_TESTNET_FORK_HEIGHT   196288 // 196288 % 64 = 0

typedef struct rx_pool_memory {
    xdag_hash_t seed;
    uint64_t seed_height;
    xtime_t seed_time;
    xdag_frame_t switch_time;
    int is_switched;
    randomx_cache *rx_cache;
    randomx_dataset *rx_dataset;
    randomx_vm *pool_vm; // for pool verify received share hash
    randomx_vm *block_vm; // for add_block_noblock
}rx_pool_mem;

#ifdef __cplusplus
extern "C"
{
#endif
extern rx_pool_mem g_rx_pool_mem[2];
extern uint64_t g_rx_pool_mem_index; // global index of pool randomx memory (dataset, cache), increased by switch time
extern uint64_t g_rx_hash_epoch_index; // global index of randomx seed, increased by blocks height
extern xdag_frame_t g_rx_fork_time;

extern int is_randomx_fork(xdag_frame_t);
// set randomx fork time and next seed switch time
extern void rx_set_fork_time(struct block_internal *m);
// unset randomx fork time and next seed switch time
extern void rx_unset_fork_time(struct block_internal *m);
//init randomx flags
extern void rx_init_flags(int is_full_mem, uint32_t init_thread_count);
//init randomx dataset for mining
extern void rx_mine_init_dataset(void *seed_data, size_t seed_size);
//randomx mining worker
extern uint64_t xdag_rx_mine_worker_hash(xdag_hash_t pre_hash, xdag_hash_t last_field ,uint64_t *nonce,
                                         uint64_t attempts, int step, xdag_hash_t hash);
//randomx hash for pool verify mining share
extern int rx_pool_calc_hash(void* data,size_t data_size,xdag_frame_t task_time,void* output_hash);
//randomx hash for calculate block difficulty
extern int rx_block_hash(void* data,size_t data_size,xdag_frame_t block_time,void* output_hash);
// create & update pool randomx cache, vm , dataset
extern int rx_pool_update_seed(uint64_t mem_index);
// release pool randomx vm, cache and dataset
extern void rx_pool_release_mem(void);
// load randomx fork time and init randomx when pool's restart with height greater than fork height
extern void rx_loading_fork_time(void);
#ifdef __cplusplus
}
#endif

#endif //XDAG_RX_HASH_H
