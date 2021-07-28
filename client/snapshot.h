//
// Created by swordlet on 2021/5/3.
//

#ifndef XDAG_SNAPSHOT_H
#define XDAG_SNAPSHOT_H
#include "lmdb/lmdb.h"
#include "block.h"

struct stats_data {
    uint64_t height; // 8
    xtime_t time; // 8
    xdag_hash_t hash; // 32
    xdag_diff_t difficulty; //16
};

struct balance_data {
    uint16_t flags; // 8bytes
    xdag_amount_t amount; //8bytes
    xtime_t time; //8bytes
    xdag_hash_t hash; //32bytes
    uint64_t storage_pos; //8
};

struct snapshot_balances_data {
    struct balance_data *blocks;
    unsigned blocksCount, maxBlocksCount;
};

extern MDB_env *g_mdb_pub_key_env;
extern MDB_env *g_mdb_balance_env;
extern MDB_dbi g_pub_key_dbi;
extern MDB_dbi g_signature_dbi;
extern MDB_dbi g_block_dbi;
extern MDB_dbi g_balance_dbi;
extern MDB_dbi g_stats_dbi;
extern MDB_txn *g_mdb_pub_key_txn;
extern MDB_txn *g_mdb_balance_txn;

int init_mdb_pub_key(void);
int init_mdb_balance(int);
int snapshot_stats(void);
int load_stats_snapshot(void);

#endif //XDAG_SNAPSHOT_H
