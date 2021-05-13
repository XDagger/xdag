//
// Created by swordlet on 2021/5/3.
//
#include <stddef.h>
#include "utils/log.h"
#include "snapshot.h"

MDB_env *g_mdb_pub_key_env;
MDB_env *g_mdb_balance_env;
MDB_dbi g_pub_key_dbi;
MDB_dbi g_balance_dbi;
MDB_txn *g_mdb_pub_key_txn;
MDB_txn *g_mdb_balance_txn;
//MDB_stat g_mdb_mst;
//MDB_cursor *g_mdb_cursor;

int init_mdb_pub_key(void) {
    if(mdb_env_create(&g_mdb_pub_key_env)) {
        xdag_mess("mdb_env_create error");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_pub_key_env, 1)) {
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
    if(mdb_txn_begin(g_mdb_pub_key_env, NULL, 0, &g_mdb_pub_key_txn)) {
        xdag_mess("mdb_txn_begin error");
        return -1;
    }
    if(mdb_dbi_open(g_mdb_pub_key_txn, "pubkey", MDB_CREATE, &g_pub_key_dbi)) {
        xdag_mess("mdb_dbi_open pub key error");
        return -1;
    }
    return 0;
}

int init_mdb_balance(void) {
    if(mdb_env_create(&g_mdb_balance_env)) {
        xdag_mess("mdb_env_create error");
        return -1;
    }
    if(mdb_env_set_maxreaders(g_mdb_balance_env, 1)) {
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
    if(mdb_env_open(g_mdb_balance_env, "./snapshot/balance", MDB_FIXEDMAP|MDB_NOSYNC, 0664)) {
        xdag_mess("mdb_env_open error");
        return -1;
    }
    if(mdb_txn_begin(g_mdb_balance_env, NULL, 0, &g_mdb_balance_txn)) {
        xdag_mess("mdb_txn_begin error");
        return -1;
    }
    if(mdb_dbi_open(g_mdb_balance_txn, "balance", MDB_CREATE, &g_balance_dbi)) {
        xdag_mess("mdb_dbi_open balance error");
        return -1;
    }
    return 0;
}
