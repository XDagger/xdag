//
// copy from mac on 2020/7/29.
//

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <randomx.h>
#include "utils/log.h"
#include "rx_hash.h"

xdag_frame_t g_rx_fork_time = -1;
uint64_t g_rx_fork_seed_height;
uint64_t g_rx_fork_lag;

uint64_t g_rx_pool_mem_index = 0;
uint64_t g_rx_hash_epoch_index = 0;
rx_pool_mem g_rx_pool_mem[2];   // two randomx seeds cover 8192 main blocks(about 6 days)

static randomx_flags g_randomx_flags = RANDOMX_FLAG_DEFAULT;

static pthread_rwlock_t  g_rx_memory_rwlock[2] = {
        PTHREAD_RWLOCK_INITIALIZER,
        PTHREAD_RWLOCK_INITIALIZER
}; //rwlock for init & update g_rx_pool_mem

//struct for muti-thread init dataset
typedef struct randomx_seedinfo {
    randomx_cache *si_cache;
    unsigned long si_start;
    unsigned long si_count;
    randomx_dataset *si_dataset;
} rx_seed_info;

//global randomx miner virables
static randomx_cache *g_rx_mine_cache = NULL;
static randomx_dataset *g_rx_mine_dataset = NULL;
static uint32_t g_mine_n_threads;

static pthread_mutex_t g_rx_dataset_mutex = PTHREAD_MUTEX_INITIALIZER;

int rx_update_vm(randomx_vm **vm, randomx_cache *cache, randomx_dataset *dataset);

static void rx_abort(const char *msg){
    fprintf(stderr, "%s\n", msg);
#ifdef NDEBUG
    _exit(1);
#else
    abort();
#endif
}
inline int is_randomx_fork(xdag_frame_t t) {
    return (g_xdag_mine_type == XDAG_RANDOMX && t > g_rx_fork_time);
}

inline void  rx_set_fork_time(struct block_internal *m) {
    uint64_t seed_epoch = g_xdag_testnet ? SEEDHASH_EPOCH_TESTNET_BLOCKS : SEEDHASH_EPOCH_BLOCKS;
    seed_epoch -= 1; // 15:4095
    if (m->height>= g_rx_fork_seed_height) {
       uint64_t next_mem_index = g_rx_hash_epoch_index + 1;
       rx_pool_mem *next_rx_mem = &g_rx_pool_mem[next_mem_index & 1];
        if (m->height == g_rx_fork_seed_height) {
            // node start before g_rx_fork_seed_height
            g_rx_fork_time = MAIN_TIME(m->time) + g_rx_fork_lag;
            xdag_info("*#*from %llu,%llx, set fork time to %llx", m->height, m->time, g_rx_fork_time);
        }
        xdag_hashlow_t hash = {0};
        if ((m->height & seed_epoch) == 0 ) {
            next_rx_mem->switch_time = MAIN_TIME(m->time) + g_rx_fork_lag + 1;
            next_rx_mem->seed_time = m->time;
            next_rx_mem->seed_height = m->height;
            xdag_info("*#*from %llu,%llx, set switch time to %llx", m->height, m->time,
                      next_rx_mem->switch_time);
            if (!xd_rsdb_get_heighthash(m->height - g_rx_fork_lag, hash) &&
                    xdag_cmphash(next_rx_mem->seed, hash) != 0) {
                // to avoid main block roll back, get prior 128 height hash as seed
                memcpy(next_rx_mem->seed, hash, sizeof(xdag_hashlow_t));
                rx_pool_update_seed(next_mem_index);
            }
            g_rx_hash_epoch_index = next_mem_index;
            next_rx_mem->is_switched = 0;
        }
   }
}

inline void  rx_unset_fork_time(struct block_internal *m) {
    uint64_t seed_epoch = g_xdag_testnet ? SEEDHASH_EPOCH_TESTNET_BLOCKS : SEEDHASH_EPOCH_BLOCKS;
    seed_epoch -= 1; // 15:4095
    if (m->height>= g_rx_fork_seed_height) {
        if (m->height == g_rx_fork_seed_height) {
            xdag_info("*#*%llu,%llx, unset fork time  %llx", m->height, m->time, g_rx_fork_time);
            g_rx_fork_time = -1;
        }
        if ((m->height & seed_epoch) == 0) {
            rx_pool_mem *rx_mem = &g_rx_pool_mem[g_rx_hash_epoch_index & 1];
            xdag_info("*#*%llu,%llx, unset switch time   %llx", m->height, m->time, rx_mem->switch_time);
            g_rx_hash_epoch_index -= 1;
            rx_mem->seed_time = -1;
            rx_mem->seed_height = -1;
            rx_mem->switch_time = -1;
            rx_mem->is_switched = -1;
        }
    }
}

void rx_init_flags(int is_full_mem, uint32_t init_thread_count) {
    if(g_xdag_testnet) {
        g_rx_fork_seed_height = RANDOMX_TESTNET_FORK_HEIGHT;
        g_rx_fork_lag = SEEDHASH_EPOCH_TESTNET_LAG;
    } else {
        g_rx_fork_seed_height = RANDOMX_FORK_HEIGHT;
        g_rx_fork_lag = SEEDHASH_EPOCH_LAG;
    }
    uint64_t seed_epoch = g_xdag_testnet ? SEEDHASH_EPOCH_TESTNET_BLOCKS : SEEDHASH_EPOCH_BLOCKS;
    if ((g_rx_fork_seed_height & (seed_epoch -1)) != 0) {
        rx_abort("randomx: invalid seed height.");
        return;
    }

    memset(g_rx_pool_mem, 0, sizeof(rx_pool_mem)*2);
    g_rx_pool_mem[0].switch_time = -1;
    g_rx_pool_mem[0].is_switched = -1;
    g_rx_pool_mem[1].switch_time = -1;
    g_rx_pool_mem[1].is_switched = -1;


    g_randomx_flags = randomx_get_flags();

    g_randomx_flags |= RANDOMX_FLAG_LARGE_PAGES;
    if (is_full_mem == 1) {
        g_randomx_flags |= RANDOMX_FLAG_FULL_MEM;
    }


    // printf randomx flags info
    if (g_randomx_flags & RANDOMX_FLAG_ARGON2_AVX2) {
        xdag_info(" - randomx Argon2 implementation: AVX2");
    } else if (g_randomx_flags & RANDOMX_FLAG_ARGON2_SSSE3) {
        xdag_info(" - randomx Argon2 implementation: SSSE3");
    } else {
        xdag_info(" - randomx Argon2 implementation: reference");
    }

    if (g_randomx_flags & RANDOMX_FLAG_FULL_MEM) {
        xdag_info(" - randomx full memory mode (2080 MiB)");
    } else {
        xdag_info(" - randomx light memory mode (256 MiB)");
    }

    if (g_randomx_flags & RANDOMX_FLAG_JIT) {
        xdag_info(" - JIT compiled mode");
        if (g_randomx_flags & RANDOMX_FLAG_SECURE) {
            xdag_info("(randomx secure)");
        }
    } else {
        xdag_info(" - randomx interpreted mode");
    }

    if (g_randomx_flags & RANDOMX_FLAG_HARD_AES) {
        xdag_info(" - randomx hardware AES mode");
    } else {
        xdag_info(" - randomx software AES mode");
    }

    if (g_randomx_flags & RANDOMX_FLAG_LARGE_PAGES) {
        xdag_info(" - randomx large pages mode");
    } else {
        xdag_info(" - randomx small pages mode");
    }

    if (g_randomx_flags & RANDOMX_FLAG_JIT) {
        xdag_info(" - randomx JIT compiled mode");
    } else {
        xdag_info(" - randomx interpreted mode");
    }
    if (init_thread_count > 0) {
        g_mine_n_threads = init_thread_count;
        xdag_info("Initializing rx mine flag (%d  thread %s ...",
                  init_thread_count, init_thread_count > 1 ? "s)" : ")");
    }

}

static void * rx_seed_thread(void *arg) {
    rx_seed_info *si = (rx_seed_info*)arg;
    xdag_info("init mine dataset with thread %lu start %lu count %lu",pthread_self(), si->si_start,si->si_count);
    randomx_init_dataset(si->si_dataset, si->si_cache, si->si_start, si->si_count);
    return NULL;
}

void rx_mine_init_dataset(void *seed_data, size_t seed_size) {
    pthread_mutex_lock(&g_rx_dataset_mutex);

    if (g_rx_mine_dataset == NULL) {
        g_rx_mine_cache = randomx_alloc_cache(g_randomx_flags);
        if (g_rx_mine_cache == NULL) {
            rx_abort("rx mine can not alloc mine cache");
            pthread_mutex_unlock(&g_rx_dataset_mutex);
            return;
        }
        g_rx_mine_dataset = randomx_alloc_dataset(g_randomx_flags);
        if (g_rx_mine_dataset == NULL) {
            rx_abort("rx mine alloc dataset failed");
            pthread_mutex_unlock(&g_rx_dataset_mutex);
            return;
        }
        xdag_info("Mine dataset is allocated");
    }
    randomx_init_cache(g_rx_mine_cache, seed_data, seed_size);

    if (g_mine_n_threads > 1) {
        unsigned long start = 0;
        int i;
        rx_seed_info *si;
        pthread_t *st;

        si = (rx_seed_info*)malloc(g_mine_n_threads * sizeof(rx_seed_info));
        if (si == NULL){
            rx_abort("Couldn't allocate RandomX mining threadinfo");
            pthread_mutex_unlock(&g_rx_dataset_mutex);
            return;
        }

        st = (pthread_t*)malloc(g_mine_n_threads * sizeof(pthread_t));
        if (st == NULL) {
            free(si);
            rx_abort("Couldn't allocate RandomX mining threadlist");
            pthread_mutex_unlock(&g_rx_dataset_mutex);
            return;
        }

        // number of items per thread
        uint32_t item_count = randomx_dataset_item_count();
        uint32_t item_count_per = item_count / g_mine_n_threads;
        uint32_t item_count_remain = item_count % g_mine_n_threads;

        xdag_info("miner init dataset count %u per %u remain %u",item_count,item_count_per,item_count_remain);

        //assign item_count items per thread to init，the last thread init item_count_remain items
        for (i=0; i < g_mine_n_threads; i++) {
            uint32_t count = item_count_per + (i == g_mine_n_threads - 1 ? item_count_remain : 0);
            si[i].si_cache = g_rx_mine_cache;
            si[i].si_start = start;   //start index
            si[i].si_count = count;   //items count
            si[i].si_dataset = g_rx_mine_dataset;
            start += count;
        }

        // create threads, every thread init a part of dataset
        for (i=0; i < g_mine_n_threads; i++) {
            pthread_create(&st[i], NULL, rx_seed_thread, &si[i]);
        }

        // waiting for threads stop
        for (i=0; i < g_mine_n_threads; i++) {
            pthread_join(st[i],NULL);
        }
        free(st);
        free(si);
    } else {
        // no multi threads, init all dataset items
        randomx_init_dataset(g_rx_mine_dataset, g_rx_mine_cache, 0, randomx_dataset_item_count());
    }
    pthread_mutex_unlock(&g_rx_dataset_mutex);
}

// hash worker per mining thread
uint64_t xdag_rx_mine_worker_hash(xdag_hash_t pre_hash, xdag_hash_t last_field ,uint64_t *nonce,
                                  uint64_t attemps, int step, xdag_hash_t output_hash){

    int pos=sizeof(xdag_hash_t) + sizeof(xdag_hashlow_t);
    xdag_hash_t hash0;
    uint64_t min_nonce=*nonce;

    if (g_rx_mine_dataset == NULL) {
        rx_abort("rx mine worker dataset is null");
        return -1;
    }

    randomx_vm *vm = randomx_create_vm(g_randomx_flags, NULL, g_rx_mine_dataset);
    if (vm == NULL) {
        rx_abort("rx mine worker alloc vm failed");
        return -1;
    }
    last_field[3]=min_nonce;
    uint8_t data2hash[sizeof(xdag_hash_t)+sizeof(xdag_hash_t)];
    memcpy(data2hash,pre_hash,sizeof(xdag_hash_t));
    memcpy(data2hash+sizeof(xdag_hash_t),last_field,sizeof(xdag_hash_t));

    randomx_calculate_hash_first(vm, data2hash, sizeof(data2hash));
    memset(output_hash, 0xff, sizeof(xdag_hash_t));

    for(int i=0;i < attemps;i++)
    {
        uint64_t curNonce = *nonce;
        *nonce += step;

        memcpy(data2hash+pos,nonce,sizeof(uint64_t));
        randomx_calculate_hash_next(vm, data2hash, sizeof(data2hash), hash0);
        if(xdag_cmphash(hash0,output_hash) < 0) {
            memcpy(output_hash, hash0, sizeof(xdag_hash_t));
            min_nonce = curNonce;
        }
    }
    randomx_destroy_vm(vm);

    xdag_info("*#*# rx final min hash %016llx%016llx%016llx%016llx",
              output_hash[3],output_hash[2],output_hash[1],output_hash[0]);
    xdag_info("*#*# rx final min nonce %016llx",min_nonce);

    return min_nonce;
}

// using multi threads to init pool dataset item
static void rx_pool_init_dataset(randomx_cache *rx_cache, randomx_dataset *rx_dataset,const int thread_count) {
    if (thread_count > 1) {
        unsigned long start = 0;
        int i;
        rx_seed_info *si;
        pthread_t *st;

        si = (rx_seed_info*)malloc(thread_count * sizeof(rx_seed_info));
        if (si == NULL){
            xdag_fatal("Couldn't allocate RandomX seed threadinfo");
            return;
        }

        st = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
        if (st == NULL) {
            free(si);
            xdag_fatal("Couldn't allocate RandomX seed threadlist");
            return;
        }

        // number of items per thread
        uint32_t item_count = randomx_dataset_item_count();
        uint32_t item_count_per = item_count / thread_count;
        uint32_t item_count_remain = item_count % thread_count;

        xdag_info("pool init dataset count %u per %u remain %u",item_count,item_count_per,item_count_remain);

        //assign item_count items per thread to init，the last thread init item_count_remain items
        for (i=0; i < thread_count; i++) {
            uint32_t count = item_count_per + (i == thread_count - 1 ? item_count_remain : 0);
            si[i].si_cache = rx_cache;
            si[i].si_start = start;   //start index
            si[i].si_count = count;   //items count
            si[i].si_dataset = rx_dataset;
            start += count;
        }

        xdag_info("init dataset for pool with %d thread",thread_count);
        // create threads, every thread init a part of dataset
        for (i=0; i < thread_count; i++) {
            pthread_create(&st[i], NULL, rx_seed_thread, &si[i]);
        }

        // waiting for threads stop
        for (i=0; i < thread_count; i++) {
            pthread_join(st[i],NULL);
        }
        free(st);
        free(si);

    } else {
        // no multi threads, init all dataset items
        randomx_init_dataset(rx_dataset, rx_cache, 0, randomx_dataset_item_count());
    }
}

// pool verify share hash from miner
int rx_pool_calc_hash(void* data,size_t data_size,xdag_frame_t task_time,void* output_hash) {
    rx_pool_mem *rx_memory = &g_rx_pool_mem[g_rx_pool_mem_index & 1];
    pthread_rwlock_t *rwlock;
    if (task_time < rx_memory->switch_time) {
        // data time is before switch time, using previous seed
        rwlock = &g_rx_memory_rwlock[(g_rx_pool_mem_index-1) & 1];
        rx_memory = &g_rx_pool_mem[(g_rx_pool_mem_index-1) & 1];
    } else {
        rwlock = &g_rx_memory_rwlock[g_rx_pool_mem_index & 1];
    }

    pthread_rwlock_rdlock(rwlock);
    randomx_calculate_hash(rx_memory->pool_vm,data,data_size,output_hash);
    pthread_rwlock_unlock(rwlock);

    return 0;
}

// randomx hash for calculate block difficulty used by add_block_nolock
int rx_block_hash(void* data,size_t data_size,xdag_frame_t block_time,void* output_hash) {
    pthread_rwlock_t *rwlock;
    rx_pool_mem *rx_memory;

    if (g_rx_hash_epoch_index == 0) { // no seed
        xdag_info("#!!! rx hash epoch index is 0");
        return -1;
    } else if (g_rx_hash_epoch_index == 1) { // first seed
        rx_memory = &g_rx_pool_mem[g_rx_hash_epoch_index & 1];
        if (block_time < rx_memory->switch_time) { // before first seed
            xdag_info("#!!! block time %16llx less than switch time %16llx", block_time, rx_memory->switch_time);
            return -1;
        } else {
            rwlock = &g_rx_memory_rwlock[g_rx_hash_epoch_index & 1];
        }
    } else {
        rx_memory = &g_rx_pool_mem[g_rx_hash_epoch_index & 1];
        if (block_time < rx_memory->switch_time) {
            rwlock = &g_rx_memory_rwlock[(g_rx_hash_epoch_index-1) & 1];
            rx_memory = &g_rx_pool_mem[(g_rx_hash_epoch_index-1) & 1];
        } else {
            rwlock = &g_rx_memory_rwlock[g_rx_hash_epoch_index & 1];
        }
    }

    pthread_rwlock_rdlock(rwlock);
    randomx_calculate_hash(rx_memory->block_vm, data, data_size, output_hash);
    pthread_rwlock_unlock(rwlock);

    return 0;
}

// update randomx vm with new seed
int rx_update_vm(randomx_vm **vm, randomx_cache *cache, randomx_dataset *dataset) {
    if(*vm == NULL){
        if (g_randomx_flags & RANDOMX_FLAG_FULL_MEM) {
            *vm = randomx_create_vm(g_randomx_flags, NULL, dataset);
        } else {
            *vm = randomx_create_vm(g_randomx_flags, cache, NULL);
        }
        if (*vm== NULL) {
//            pthread_rwlock_unlock(rwlock);
            return -1;
        }
        xdag_info(" vm created");
    } else {
        if (g_randomx_flags & RANDOMX_FLAG_FULL_MEM) {
            randomx_vm_set_dataset(*vm, dataset);
        } else {
            randomx_vm_set_cache(*vm, cache);
        }
        xdag_info(" vm updated");
    }
    return 0;
}

int rx_pool_update_seed(uint64_t mem_index) {

    pthread_rwlock_t *rwlock = &g_rx_memory_rwlock[mem_index & 1];
    pthread_rwlock_wrlock(rwlock);
    rx_pool_mem *rx_memory = &g_rx_pool_mem[mem_index & 1];

    if (rx_memory->rx_cache == NULL){
        xdag_info("alloc pool rx cache ...");
        rx_memory->rx_cache = randomx_alloc_cache(g_randomx_flags);
        if (rx_memory->rx_cache == NULL) {
            pthread_rwlock_unlock(rwlock);
            rx_abort("alloc rx cache failed");
            return -1;
        }
    }

    if (rx_memory->rx_cache != NULL) {
        xdag_fatal("update pool rx cache ...");
        randomx_init_cache(rx_memory->rx_cache, rx_memory->seed, sizeof(rx_memory->seed));
    }
    if (g_randomx_flags & RANDOMX_FLAG_FULL_MEM) {
        if (rx_memory->rx_dataset == NULL) {
            xdag_info("alloc pool rx dataset ...");
            rx_memory->rx_dataset = randomx_alloc_dataset(g_randomx_flags);
            if (rx_memory->rx_dataset == NULL) {
                pthread_rwlock_unlock(rwlock);
                rx_abort("alloc dataset failed");
                return -1;
            }
        }
        if (rx_memory->rx_dataset != NULL) {
            xdag_info("update pool rx dataset ...");
            rx_pool_init_dataset(rx_memory->rx_cache, rx_memory->rx_dataset, 4);
        }
    }

    if (rx_update_vm(&rx_memory->pool_vm, rx_memory->rx_cache, rx_memory->rx_dataset) < 0) {
        pthread_rwlock_unlock(rwlock);
        rx_abort("update pool vm failed");
        return -1;
    }
    xdag_info("update pool vm finished");

    if (rx_update_vm(&rx_memory->block_vm, rx_memory->rx_cache, rx_memory->rx_dataset) < 0) {
        rx_abort("update block vm failed");
        pthread_rwlock_unlock(rwlock);
        return -1;
    }
    xdag_info("update block vm finished");

    pthread_rwlock_unlock(rwlock);
    return 0;
}

//void rx_miner_release_mem(void) {
//    pthread_mutex_lock(&g_rx_dataset_mutex);
//
//    if (g_rx_mine_cache != NULL) {
//        randomx_release_cache(g_rx_mine_cache);
//    }
//    if(g_rx_mine_dataset != NULL) {
//        randomx_release_dataset(g_rx_mine_dataset);
//    }
//
//    pthread_mutex_unlock(&g_rx_dataset_mutex);
//}

void rx_pool_release_mem(void) {
    for (int i = 0; i < 2; ++i) {
        pthread_rwlock_t *rwlock = &g_rx_memory_rwlock[i];
        pthread_rwlock_wrlock(rwlock);
        rx_pool_mem *rx_memory = &g_rx_pool_mem[i];
        if(rx_memory->pool_vm != NULL) {
            randomx_destroy_vm(rx_memory->pool_vm);
        }
        if(rx_memory->block_vm != NULL) {
            randomx_destroy_vm(rx_memory->block_vm);
        }
        if(rx_memory->rx_cache != NULL) {
            randomx_release_cache(rx_memory->rx_cache);
        }
        if(rx_memory->rx_dataset != NULL) {
            randomx_release_dataset(rx_memory->rx_dataset);
        }
        pthread_rwlock_unlock(rwlock);
    }
}

void rx_loading_fork_time(void) {    // node start height greater than g_rx_fork_seed_height
    xdag_hashlow_t hash = {0};
    struct block_internal b;
    xdag_hashlow_t hash_seed = {0};
    if (g_xdag_stats.nmain >= g_rx_fork_seed_height) {
        if (!xd_rsdb_get_heighthash(g_rx_fork_seed_height, hash)) {

            if (!xd_rsdb_get_bi(hash, &b)) {
                g_rx_fork_time = MAIN_TIME(b.time) + g_rx_fork_lag;
                xdag_info("loading fork time to %16llx", g_rx_fork_time);
            }
        }
        uint64_t seed_epoch = g_xdag_testnet ? SEEDHASH_EPOCH_TESTNET_BLOCKS : SEEDHASH_EPOCH_BLOCKS;
        seed_epoch -= 1; // 15:4095
        uint64_t seed_height = g_xdag_stats.nmain & ~seed_epoch ;
        uint64_t pre_seed_height = seed_height - seed_epoch - 1;

        if (pre_seed_height >= g_rx_fork_seed_height) {
            if (!xd_rsdb_get_heighthash(pre_seed_height, hash) &&
                    !xd_rsdb_get_heighthash(pre_seed_height - g_rx_fork_lag, hash_seed)) {
                if (!xd_rsdb_get_bi(hash, &b)) {
                    uint64_t mem_index = g_rx_hash_epoch_index + 1;
                    rx_pool_mem *rx_mem = &g_rx_pool_mem[mem_index & 1];
                    memcpy(rx_mem->seed, hash_seed, sizeof(xdag_hashlow_t));
                    rx_mem->switch_time = MAIN_TIME(b.time) + g_rx_fork_lag + 1;
                    rx_mem->seed_time = b.time;
                    rx_mem->seed_height = b.height;
                    xdag_info("loading previous rx pool mem %llu,%llx, set switch time to %llx", b.height, b.time,
                              rx_mem->switch_time);
                    rx_pool_update_seed(mem_index);
                    g_rx_hash_epoch_index = mem_index;
                    rx_mem->is_switched = 1;
                }
            }
        }
        if (seed_height >= g_rx_fork_seed_height) {
            if (!xd_rsdb_get_heighthash(seed_height, hash) &&
                    !xd_rsdb_get_heighthash(seed_height - g_rx_fork_lag, hash_seed)) {
                if (!xd_rsdb_get_bi(hash, &b)) {
                    uint64_t mem_index = g_rx_hash_epoch_index + 1;
                    rx_pool_mem *rx_mem = &g_rx_pool_mem[mem_index & 1];
                    memcpy(rx_mem->seed, hash_seed, sizeof(xdag_hashlow_t));
                    rx_mem->switch_time = MAIN_TIME(b.time) + g_rx_fork_lag + 1;
                    rx_mem->seed_time = b.time;
                    rx_mem->seed_height = b.height;
                    xdag_info("loading current rx pool mem %llu,%llx, set switch time to %llx", b.height, b.time,
                              rx_mem->switch_time);
                    rx_pool_update_seed(mem_index);
                    g_rx_hash_epoch_index = mem_index;
                    rx_mem->is_switched = 0;
                }
            }
        }
    }
}
