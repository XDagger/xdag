//
// Created by swordlet on 2021/5/3.
//
#include <string.h>
#include "address.h"
#include "utils/log.h"
#include "snapshot.h"
#include "global.h"

MDB_env *g_mdb_pub_key_env;
MDB_env *g_mdb_balance_env;
MDB_dbi g_pub_key_dbi;
MDB_dbi g_balance_dbi;
MDB_dbi g_signature_dbi;
MDB_dbi g_stats_dbi;
MDB_txn *g_mdb_pub_key_txn;
MDB_txn *g_mdb_balance_txn;
//MDB_stat g_mdb_mst;
//MDB_cursor *g_mdb_cursor;

char key_snapshot_time[]={"g_snapshot_time"};
char key_snapshot_main[]={"g_snapshot_main"};
char key_snapshot_height[]={"g_snapshot_height"};
char key_snapshot_difficulty[]={"g_snapshot_difficulty"};

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

    //key_snapshot_main
    struct block_internal *bi = block_by_height(g_steps_height[g_steps_index]);
    if(bi == NULL) {
        printf("get main block by snapshot height error\n");
        return -1;
    }

    mdb_key.mv_data = key_snapshot_main;
    mdb_key.mv_size = sizeof(key_snapshot_main);
    mdb_data.mv_data = bi->hash;
    mdb_data.mv_size = sizeof(xdag_hash_t);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        printf("mdb put main block error\n");
        return -1;
    }

    //key_snapshot_time
    mdb_key.mv_data = key_snapshot_time;
    mdb_key.mv_size = sizeof(key_snapshot_time);
    mdb_data.mv_data = &bi->time;
    mdb_data.mv_size = sizeof(uint64_t);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        printf("mdb put key_snapshot_time error\n");
        return -1;
    }

    //key_snapshot_difficulty
    mdb_key.mv_data = key_snapshot_difficulty;
    mdb_key.mv_size = sizeof(key_snapshot_difficulty);
    mdb_data.mv_data = &bi->difficulty;
    mdb_data.mv_size = sizeof(xdag_diff_t);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        printf("mdb put g_snapshot_difficulty error\n");
        return -1;
    }

    //key_snapshot_height
    mdb_key.mv_data = key_snapshot_height;
    mdb_key.mv_size = sizeof(key_snapshot_height);
    mdb_data.mv_data = &bi->height;
    mdb_data.mv_size = sizeof(uint64_t);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        printf("mdb put g_snapshot_height error\n");
        return -1;
    }

    return 0;
}

int load_stats_snapshot() {
    MDB_val mdb_key, mdb_data;
    int res;
    //key_snapshot_time
    mdb_key.mv_data = key_snapshot_time;
    mdb_key.mv_size = sizeof(key_snapshot_time);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        printf("mdb get key_snapshot_time error\n");
        return -1;
    }
    g_snapshot_time = *((uint64_t *)mdb_data.mv_data);

    //key_snapshot_main
    char address[33] = {0};
    mdb_key.mv_data = key_snapshot_main;
    mdb_key.mv_size = sizeof(key_snapshot_main);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        printf("mdb get key_snapshot_main error\n");
        return -1;
    }
    xdag_hash2address((uint64_t *) mdb_data.mv_data, address);

    printf("main=%s time=%lx\n", address, g_snapshot_time);

    //key_xdag_state
    mdb_key.mv_data = key_snapshot_difficulty;
    mdb_key.mv_size = sizeof(key_snapshot_difficulty);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        printf("mdb get key_snapshot_difficulty error\n");
        return -1;
    }
    xdag_diff_t main_diff = *((xdag_diff_t *)mdb_data.mv_data);

    //key_xdag_stats
    mdb_key.mv_data = key_snapshot_height;
    mdb_key.mv_size = sizeof(key_snapshot_height);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        printf("mdb get key_snapshot_height error\n");
        return -1;
    }
    uint64_t main_height = *((uint64_t *)mdb_data.mv_data);

    printf("main diff=%llx%016llx  main height=%ld\n", xdag_diff_args(main_diff), main_height);

    return 0;
}
