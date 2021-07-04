//
// Created by swordlet on 2021/5/3.
//
#include <string.h>
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
char key_xdag_stats[]={"g_xdag_stats"};
char key_xdag_extstats[]={"g_xdag_extstats"};
char key_xdag_state[]={"g_xdag_state"};

int init_mdb_pub_key(void) {
    if(mdb_env_create(&g_mdb_pub_key_env)) {
        xdag_mess("mdb_env_create error");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_pub_key_env, 4)) {
        xdag_mess("mdb_env_set_maxreaders error");
        return -1;
    }
    if(mdb_env_set_mapsize(g_mdb_pub_key_env, 268435456)) {
        xdag_mess("mdb_env_set_mapsize error");
        return -1;
    }
    if(mdb_env_set_maxdbs(g_mdb_pub_key_env, 4)) {
        xdag_mess("mdb_env_set_maxdbs error");
        return -1;
    }
    if(mdb_env_open(g_mdb_pub_key_env, "./snapshot/pubkey", MDB_FIXEDMAP|MDB_NOSYNC, 0664)) {
        xdag_mess("mdb_env_open error");
        return -1;
    }
    return 0;
}

int init_mdb_balance(int height) {
    if(mdb_env_create(&g_mdb_balance_env)) {
        xdag_mess("mdb_env_create error");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_balance_env, 4)) {
        xdag_mess("mdb_env_set_maxreaders error");
        return -1;
    }
    if(mdb_env_set_mapsize(g_mdb_balance_env, 268435456)) {
        xdag_mess("mdb_env_set_mapsize error");
        return -1;
    }
    if(mdb_env_set_maxdbs(g_mdb_balance_env, 4)) {
        xdag_mess("mdb_env_set_maxdbs error");
        return -1;
    }
    char path[256] = {0};
    sprintf(path,"./snapshot/balance/%d", height);
    if(mdb_env_open(g_mdb_balance_env, path, MDB_FIXEDMAP|MDB_NOSYNC, 0664)) {
        xdag_mess("mdb_env_open error");
        return -1;
    }
    return 0;
}

int snapshot_stats() {
    MDB_val mdb_key, mdb_data;
    int res;

    //key_snapshot_time
    mdb_key.mv_data = key_snapshot_time;
    mdb_key.mv_size = sizeof(key_snapshot_time);
    mdb_data.mv_data = &g_snapshot_time;
    mdb_data.mv_size = sizeof(g_snapshot_time);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        xdag_mess("mdb put key_snapshot_time error");
        return -1;
    }

    //key_xdag_stats
    mdb_key.mv_data = key_xdag_stats;
    mdb_key.mv_size = sizeof(key_xdag_stats);
    mdb_data.mv_data = &g_xdag_stats;
    mdb_data.mv_size = sizeof(g_xdag_stats);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        xdag_mess("mdb put g_xdag_stats error");
        return -1;
    }

    //key_xdag_extstats
    mdb_key.mv_data = key_xdag_extstats;
    mdb_key.mv_size = sizeof(key_xdag_extstats);
    mdb_data.mv_data = &g_xdag_extstats;
    mdb_data.mv_size = sizeof(g_xdag_extstats);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        xdag_mess("mdb put g_xdag_extstats error");
        return -1;
    }

    //key_xdag_state
    mdb_key.mv_data = key_xdag_state;
    mdb_key.mv_size = sizeof(key_xdag_state);
    mdb_data.mv_data = &g_xdag_state;
    mdb_data.mv_size = sizeof(int);
    res = mdb_put(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data, MDB_NOOVERWRITE);
    if(res) {
        xdag_mess("mdb put g_xdag_state error");
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
        xdag_mess("mdb get key_snapshot_time error");
        return -1;
    }
    g_snapshot_time = *((uint64_t *)mdb_data.mv_data);

    //key_xdag_state
    mdb_key.mv_data = key_xdag_state;
    mdb_key.mv_size = sizeof(key_xdag_state);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        xdag_mess("mdb get key_xdag_state error");
        return -1;
    }
    g_xdag_state = *((int *)mdb_data.mv_data);
    printf("g_xdag_state: %08xd\n", g_xdag_state);

    //key_xdag_stats
    mdb_key.mv_data = key_xdag_stats;
    mdb_key.mv_size = sizeof(key_xdag_stats);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        xdag_mess("mdb get key_xdag_stats error");
        return -1;
    }
    memcpy(&g_xdag_stats,(struct xdag_stats*)mdb_data.mv_data, sizeof(struct xdag_stats));

    printf("g_xdag_stats.nmain: %ld\n", g_xdag_stats.nmain);

    //key_xdag_extstats
    mdb_key.mv_data = key_xdag_extstats;
    mdb_key.mv_size = sizeof(key_xdag_extstats);
    res = mdb_get(g_mdb_balance_txn, g_stats_dbi, &mdb_key, &mdb_data);
    if(res) {
        xdag_mess("mdb get key_xdag_extstats error");
        return -1;
    }
    memcpy(&g_xdag_extstats,(struct xdag_ext_stats*)mdb_data.mv_data, sizeof(struct xdag_ext_stats));
    printf("g_xdag_extstats.nextra: %ld\n", g_xdag_extstats.nextra);

    return 0;
}
