//
// Created by swordlet on 2021/5/3.
//
#include <string.h>
#include "address.h"
#include "utils/log.h"
#include "snapshot.h"
#include "global.h"
#include "rx_hash.h"


MDB_env *g_mdb_pub_key_env;
MDB_env *g_mdb_balance_env;
MDB_dbi g_pub_key_dbi;
MDB_dbi g_balance_dbi;
MDB_dbi g_signature_dbi;
MDB_dbi g_block_dbi;
MDB_dbi g_stats_dbi;
MDB_txn *g_mdb_pub_key_txn;
MDB_txn *g_mdb_balance_txn;

char key_snapshot_main[]={"g_snapshot_main"};

int init_mdb_pub_key(void) {
    if(mdb_env_create(&g_mdb_pub_key_env)) {
        printf("mdb_env_create error\n");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_pub_key_env, 4)) {
        printf("mdb_env_set_maxreaders error\n");
        return -1;
    }
    if(mdb_env_set_mapsize(g_mdb_pub_key_env, 268435456)) {
        printf("mdb_env_set_mapsize error\n");
        return -1;
    }
    if(mdb_env_set_maxdbs(g_mdb_pub_key_env, 4)) {
        printf("mdb_env_set_maxdbs error\n");
        return -1;
    }
    if(mdb_env_open(g_mdb_pub_key_env, "./snapshot/pubkey", MDB_FIXEDMAP|MDB_NOSYNC, 0664)) {
        printf("mdb_env_open error\n");
        return -1;
    }
    return 0;
}

int init_mdb_balance(int height) {
    if(mdb_env_create(&g_mdb_balance_env)) {
        printf("mdb_env_create error\n");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_balance_env, 4)) {
        printf("mdb_env_set_maxreaders error\n");
        return -1;
    }
    if(mdb_env_set_mapsize(g_mdb_balance_env, 268435456)) {
        printf("mdb_env_set_mapsize error\n");
        return -1;
    }
    if(mdb_env_set_maxdbs(g_mdb_balance_env, 4)) {
        printf("mdb_env_set_maxdbs error\n");
        return -1;
    }
    char path[256] = {0};
    sprintf(path,"./snapshot/balance/%d", height);
    if(mdb_env_open(g_mdb_balance_env, path, MDB_FIXEDMAP|MDB_NOSYNC, 0664)) {
        printf("mdb_env_open error\n");
        return -1;
    }
    return 0;
}

int snapshot_stats() {
    MDB_val mdb_key, mdb_data;
    int res;
    int snapshot_height = g_steps_height[g_steps_index];

    //key_snapshot_main
    struct block_internal *bi = block_by_height(snapshot_height);
    if(bi == NULL) {
        printf("get main block by snapshot height error\n");
        return -1;
    }
    struct stats_data block;
    mdb_key.mv_data = key_snapshot_main;
    mdb_key.mv_size = sizeof(key_snapshot_main);
    mdb_data.mv_data = &block;
    mdb_data.mv_size = sizeof(struct stats_data);
    block.time = bi->time;
    block.height = bi->height;
    memcpy(block.hash, bi->hash, sizeof(xdag_hash_t));
    block.difficulty = bi->difficulty;
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        printf("mdb put main block error\n");
        return -1;
    }

    for (int i = 1; i <= 128; i++) {
        bi = block_by_height(snapshot_height-i);
        if(bi == NULL) {
            printf("get main block by snapshot height error\n");
            return -1;
        }
        char key_val[32] = {0};
        mdb_key.mv_data = key_val;
        sprintf(key_val, "g_snapshot_main_%d", i);
        mdb_key.mv_size = strlen(key_val);
        block.time = bi->time;
        block.height = bi->height;
        memcpy(block.hash, bi->hash, sizeof(xdag_hash_t));
        block.difficulty = bi->difficulty;
        res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
        if(res) {
            printf("mdb put main block %d error\n", snapshot_height-i);
            return -1;
        }
    }

    uint64_t seed_epoch = g_xdag_testnet ? SEEDHASH_EPOCH_TESTNET_BLOCKS : SEEDHASH_EPOCH_BLOCKS;
    uint64_t lag = g_xdag_testnet?SEEDHASH_EPOCH_TESTNET_LAG:SEEDHASH_EPOCH_LAG;
    char key_val[32] = {0};
    mdb_key.mv_data = key_val;
    sprintf(key_val, "pre_seed");
    mdb_key.mv_size = strlen(key_val);
    xdag_hash_t hash = {0};
    memcpy(hash, block_by_height(snapshot_height-seed_epoch-lag-1)->hash, sizeof(xdag_hashlow_t));
    memcpy(mdb_data.mv_data,bi->seed,sizeof(xdag_hash_t));
    mdb_data.mv_size = sizeof(xdag_hash_t);
    printf("mdb put seed %016llx%016llx%016llx%016llx\n",
		(unsigned long long)bi->seed[3], (unsigned long long)bi->seed[2], (unsigned long long)bi->seed[1], (unsigned long long)bi->seed[0]);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        return -1;
    }

    return 0;
}

int load_stats_snapshot() {
    MDB_val mdb_key, mdb_data;
    int res;

    //key_snapshot_main
    char address[33] = {0};
    mdb_key.mv_data = key_snapshot_main;
    mdb_key.mv_size = sizeof(key_snapshot_main);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        printf("mdb get key_snapshot_main error\n");
        return -1;
    }
    struct stats_data *block = (struct stats_data*)mdb_data.mv_data;
    xdag_hash2address(block->hash, address);
    g_snapshot_time = block->time;
    xdag_diff_t main_diff = block->difficulty;
    uint64_t main_height = block->height;

    printf("main=%s time=%lx\n", address, g_snapshot_time);
    printf("main diff=%llx%016llx  main height=%ld\n", xdag_diff_args(main_diff), main_height);

    for (int i = 1; i <= 128; i++) {
        char key_val[32] = {0};
        mdb_key.mv_data = key_val;
        sprintf(key_val, "g_snapshot_main_%d", i);
        mdb_key.mv_size = strlen(key_val);
        res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
        if(res) {
            printf("mdb get key_snapshot_main error\n");
            return -1;
        }

        block = (struct stats_data*)mdb_data.mv_data;
        xdag_hash2address(block->hash, address);
        g_snapshot_time = block->time;
        main_diff = block->difficulty;
        main_height = block->height;

        printf("main_%d=%s time=%lx\n",i, address, g_snapshot_time);
        printf("main_%d diff=%llx%016llx  main height=%ld\n", i, xdag_diff_args(main_diff), main_height);

    }

    return 0;
}
