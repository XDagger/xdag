//
// Created by swordlet on 2021/5/3.
//

#ifndef XDAG_SNAPSHOT_H
#define XDAG_SNAPSHOT_H
#include "lmdb/lmdb.h"

extern MDB_env *g_mdb_pub_key_env;
extern MDB_env *g_mdb_balance_env;
extern MDB_dbi g_pub_key_dbi;
extern MDB_dbi g_balance_dbi;
extern MDB_txn *g_mdb_pub_key_txn;
extern MDB_txn *g_mdb_balance_txn;
//MDB_stat g_mdb_mst;
//MDB_cursor *g_mdb_cursor;

int init_mdb_pub_key(void);
int init_mdb_balance(void);

#endif //XDAG_SNAPSHOT_H
