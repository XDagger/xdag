//
// Created by swordlet on 2021/5/3.
//

#ifndef XDAG_SNAPSHOT_H
#define XDAG_SNAPSHOT_H
#include "lmdb/lmdb.h"
#include "block.h"

struct balance_data {
    xdag_amount_t amount;
    xtime_t time;
    uint64_t storage_pos;
    xdag_hash_t hash;
};

struct snapshot_balances_data {
    struct balance_data *blocks;
    unsigned blocksCount, maxBlocksCount;
};

extern MDB_env *g_mdb_pub_key_env;
extern MDB_env *g_mdb_balance_env;
extern MDB_dbi g_pub_key_dbi;
extern MDB_dbi g_balance_dbi;
extern MDB_dbi g_stats_dbi;
extern MDB_txn *g_mdb_pub_key_txn;
extern MDB_txn *g_mdb_balance_txn;
//MDB_stat g_mdb_mst;
//MDB_cursor *g_mdb_cursor;
extern char key_snapshot_time[];
extern char key_xdag_stats[];
extern char key_xdag_extstats[];
extern char key_xdag_state[];

int init_mdb_pub_key(void);
int init_mdb_balance(int);
int snapshot_stats(void);
int load_stats_snapshot(void);

#endif //XDAG_SNAPSHOT_H
