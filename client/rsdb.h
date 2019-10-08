#ifndef ROCKSDB_H
#define ROCKSDB_H

#include <stdint.h>
#include <stdio.h>
#include "types.h"
#include "hash.h"
#include "system.h"
#include "rocksdb/c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xdag_rsdb_conf {
    char*                              db_name;
    char*                              db_path;
    char*                       db_backup_path;
} XDAG_RSDB_CONF;

typedef struct xdag_rsdb {
    struct xdag_rsdb_conf*              config;
    rocksdb_options_t*                 options;
    rocksdb_readoptions_t*        read_options;
    rocksdb_writeoptions_t*      write_options;
    rocksdb_restore_options_t* restore_options;
    rocksdb_backup_engine_t*     backup_engine;
    rocksdb_t*                              db;
} XDAG_RSDB;

typedef enum xdag_rsdb_op_result {
    XDAG_RSDB_OP_SUCCESS                  =  0,
    XDAG_RSDB_NULL                        =  1,
    XDAG_RSDB_CONF_ERROR                  =  2,
    XDAG_RSDB_INIT_ERROR                  =  3,
    XDAG_RSDB_OPEN_ERROR                  =  4,
    XDAG_RSDB_CLOST_ERROR                 =  5,
    XDAG_RSDB_BKUP_ERROR                  =  6,
    XDAG_RSDB_RESTORE_ERROR               =  7,
    XDAG_RSDB_PUT_ERROR                   =  8,
    XDAG_RSDB_GET_ERROR                   =  9,
    XDAG_RSDB_DELETE_ERROR                = 10
} XDAG_RSDB_OP_TYPE;

typedef enum xdag_rsdb_key_type {
    SETTING_CREATED                       =  0,
    SETTING_MAIN_HEAD                     =  1,
    SETTING_VERIFIED_MAIN_HEAD            =  2,
    SETTING_VERSION                       =  3,
    HEADERS_ALL                           =  4,
    UNDOABLEBLOCKS_ALL                    =  5,
    HEIGHT_UNDOABLEBLOCKS                 =  6,
    OPENOUT_ALL                           =  7,
    ADDRESS_HASHINDEX                     =  8
} XDAG_RSDB_KEY_TYPE;

typedef struct xdag_utxo {
    uint64_t height;
    xdag_hash_t hash;
    uint8_t index;
    xdag_amount_t amount;
    uint64_t *pub;
} XDAG_UTXO;

typedef struct xdag_stored_block {
    xdag_hash_t hash;
    uint64_t height;
    xdag_diff_t difficulty;
} XDAG_STORED_BLOCK;

/*
 * for rocksdb
 */
extern int xdag_rsdb_check(XDAG_RSDB  *xdag_rsdb);

extern int xdag_rsdb_conf_check(XDAG_RSDB  *db);

extern int xdag_rsdb_conf(XDAG_RSDB* db);

extern int xdag_rsdb_open(XDAG_RSDB* db);

extern int xdag_rsdb_backup(XDAG_RSDB* db);

extern int xdag_rsdb_restore(XDAG_RSDB* db);

extern int xdag_rsdb_close(XDAG_RSDB* db);

extern int xdag_rsdb_create(XDAG_RSDB* db);

extern int xdag_rsdb_load(XDAG_RSDB* db);

extern int xdag_rsdb_put(XDAG_RSDB* db, const char* key, const char* value);

extern int xdag_rsdb_get(XDAG_RSDB* db, const char* key, char* data, size_t* len);

extern int xdag_rsdb_delete(const XDAG_RSDB* db, const char* key);

extern int xdag_rsdb_getkey(XDAG_RSDB_KEY_TYPE key_type, const char* key, char* new_key, size_t size);

/*
 * for xdag
 */
extern int xdag_rsdb_init(const XDAG_RSDB* db);

//size_t g_xdag_rsdb_head_nmain;

XDAG_RSDB* g_xdag_rsdb;

//xdag_block g_xdag_last_main_block;
//
//xdag_block g_xdag_verify_main_block;

#ifdef __cplusplus
};
#endif

#endif /* xdag_rocksdb_h */
