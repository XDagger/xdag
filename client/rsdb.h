#ifndef XDAG_RSDB_H
#define XDAG_RSDB_H

#include <stdint.h>
#include <stdio.h>
#include <rocksdb/c.h>
#include "types.h"
#include "hash.h"
#include "system.h"
#include "block.h"

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
    rocksdb_flushoptions_t*      flush_options;
    rocksdb_restore_options_t* restore_options;
    rocksdb_backup_engine_t*     backup_engine;
    rocksdb_t*                              db;
} XDAG_RSDB;
    
typedef struct xdag_rsdb_batch {
    rocksdb_writebatch_t* writebatch;
} XDAG_RSDB_BATCH;

typedef enum xdag_rsdb_op_result {
    XDAG_RSDB_OP_SUCCESS                  =  0,
    XDAG_RSDB_INIT_NEW                    =  1,
    XDAG_RSDB_INIT_LOAD                   =  2,
    XDAG_RSDB_KEY_NOT_EXIST               =  3,
    XDAG_RSDB_NULL                        =  4,
    XDAG_RSDB_CONF_ERROR                  =  5,
    XDAG_RSDB_INIT_ERROR                  =  6,
    XDAG_RSDB_OPEN_ERROR                  =  7,
    XDAG_RSDB_CLOST_ERROR                 =  8,
    XDAG_RSDB_BKUP_ERROR                  =  9,
    XDAG_RSDB_RESTORE_ERROR               = 10,
    XDAG_RSDB_PUT_ERROR                   = 11,
    XDAG_RSDB_GET_ERROR                   = 12,
    XDAG_RSDB_DELETE_ERROR                = 13,
    XDAG_RSDB_WRITEBATCH_CREATE_ERROR     = 14,
    XDAG_RSDB_WRITE_ERROR                 = 15,
    XDAG_RSDB_SEEK_ERROR                  = 16
} XDAG_RSDB_OP_TYPE;

typedef enum xdag_rsdb_key_type {
    SETTING_CREATED                       =  0,
    SETTING_STATS                         =  1,
    SETTING_TOP_MAIN_HASH                 =  2,
    SETTING_PRE_TOP_MAIN_HASH             =  3,
    SETTING_OUR_FIRST_HASH                =  4,
    SETTING_OUR_LAST_HASH                 =  5,
    SETTING_VERSION                       =  6,
    HASH_ORP_BLOCK_INTERNAL               =  7,
    HASH_OUR_BLOCK_INTERNAL               =  8,
    HASH_BLOCK_INTERNAL                   =  9,
    HASH_BLOCK_LINK                       =  10,
    HASH_BLOCK_BACK_REF                   =  11
} XDAG_RSDB_KEY_TYPE;

int xdag_rsdb_pre_init(void);

int xdag_rsdb_init(void);
    
int xdag_rsdb_check(XDAG_RSDB  *xdag_rsdb);

int xdag_rsdb_conf_check(XDAG_RSDB  *db);

int xdag_rsdb_conf(XDAG_RSDB* db);

int xdag_rsdb_open(XDAG_RSDB* db);

int xdag_rsdb_backup(XDAG_RSDB* db);

int xdag_rsdb_restore(XDAG_RSDB* db);

int xdag_rsdb_close(XDAG_RSDB* db);

XDAG_RSDB* xdag_rsdb_new(char* db_name, char* db_path, char* db_backup_path);

int xdag_rsdb_delete(XDAG_RSDB* db);

int xdag_rsdb_load(XDAG_RSDB* db);

int xdag_rsdb_putkey(XDAG_RSDB* db, const char* key, size_t klen, const char* value, size_t vlen);

void* xdag_rsdb_getkey(const char* key, const size_t* klen, size_t* vlen);

int xdag_rsdb_delkey(XDAG_RSDB* db, const char* key, size_t klen);

XDAG_RSDB_BATCH* xdag_rsdb_writebatch_new(void);

void xdag_rsdb_writebatch_destroy(XDAG_RSDB_BATCH* batch);
    
int xdag_rsdb_writebatch_put(XDAG_RSDB_BATCH* batch, const char* key, size_t klen,const char* value, size_t vlen);

int xdag_rsdb_write(XDAG_RSDB* db, XDAG_RSDB_BATCH* batch);

int xdag_rsdb_put_stats(XDAG_RSDB* rsdb);

struct block_internal* xdag_rsdb_get_bi(const void* hp);
struct block_internal* xdag_rsdb_get_orpbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash);
struct block_internal* xdag_rsdb_get_ourbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash);


int xdag_rsdb_put_bi(XDAG_RSDB* rsdb, struct block_internal* bi);
int xdag_rsdb_put_orpbi(XDAG_RSDB* rsdb, struct block_internal* bi);
int xdag_rsdb_put_ourbi(XDAG_RSDB* rsdb, struct block_internal* bi);

int xdag_rsdb_del_bi(XDAG_RSDB* rsdb, xdag_hashlow_t hash);
int xdag_rsdb_del_orpbi(XDAG_RSDB* rsdb, struct block_internal* bi);
int xdag_rsdb_del_ourbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash);

struct block_internal* xdag_rsdb_seek_orpbi(XDAG_RSDB* rsdb);
    
int xdag_rsdb_writebatch_put_bi(XDAG_RSDB_BATCH* batch, struct block_internal* bi);

#endif
