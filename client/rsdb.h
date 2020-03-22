#ifndef XDAG_RSDB_H
#define XDAG_RSDB_H

#include <stdint.h>
#include <stdio.h>
#include <rocksdb/c.h>
#include "types.h"
#include "hash.h"
#include "system.h"
#include "block.h"
#include "sync.h"

#define RSDB_KEY_LEN (1 + sizeof(xdag_hashlow_t))

typedef struct xdag_rsdb_conf {
    char                              *db_name;
    char                              *db_path;
} xd_rsdb_conf_t;

typedef struct xdag_rsdb {
    xd_rsdb_conf_t                     *config;
    rocksdb_options_t                 *options;
    rocksdb_readoptions_t        *read_options;
    rocksdb_writeoptions_t      *write_options;
    rocksdb_t                              *db;
} xd_rsdb_t;

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
    XDAG_RSDB_WRITE_ERROR                 = 14,
    XDAG_RSDB_SEEK_ERROR                  = 15
} xd_rsdb_op_t;

typedef enum xdag_rsdb_key_type {
    SETTING_VERSION                       =  0x00,
    SETTING_CREATED                       =  0x01,
    SETTING_STATS                         =  0x02,
    SETTING_EXT_STATS                     =  0x03,
    SETTING_PRE_TOP_MAIN_HASH             =  0x04,
    SETTING_TOP_MAIN_HASH                 =  0x05,
    SETTING_OUR_FIRST_HASH                =  0x06,
    SETTING_OUR_LAST_HASH                 =  0x07,
    HASH_ORP_BLOCK                        =  0x08,
    HASH_BLOCK_INTERNAL                   =  0x09,
    HASH_BLOCK_OUR                        =  0x0a,
    HASH_BLOCK_REMARK                     =  0x0b,
    HASH_BLOCK_SYNC                       =  0x0c,
    HASH_BLOCK_CACHE                      =  0x0d
} xd_rsdb_key_t;

xd_rsdb_op_t xdag_rsdb_pre_init(void);
xd_rsdb_op_t xdag_rsdb_init(void);
xd_rsdb_op_t xdag_rsdb_load(xd_rsdb_t* db);
xd_rsdb_op_t xdag_rsdb_conf_check(xd_rsdb_t  *db);
xd_rsdb_op_t xdag_rsdb_conf(xd_rsdb_t* db);
xd_rsdb_op_t xdag_rsdb_open(xd_rsdb_t* db);
xd_rsdb_op_t xdag_rsdb_close(xd_rsdb_t* db);

void* xdag_rsdb_getkey(const char* key, const size_t klen, size_t* vlen);
xd_rsdb_op_t xdag_rsdb_get_bi(xdag_hashlow_t hash,struct block_internal*);
xd_rsdb_op_t xdag_rsdb_get_ourbi(xdag_hashlow_t hash,struct block_internal*);
xd_rsdb_op_t xdag_rsdb_get_orpblock(xdag_hashlow_t hash, struct xdag_block*);
xd_rsdb_op_t xdag_rsdb_get_stats(void);
xd_rsdb_op_t xdag_rsdb_get_extstats(void);
xd_rsdb_op_t xdag_rsdb_get_remark(xdag_hashlow_t hash, xdag_remark_t);
xd_rsdb_op_t xdag_rsdb_get_syncblock(xdag_hashlow_t ref_hash, xdag_hashlow_t block_hash, struct sync_block*);
xd_rsdb_op_t xdag_rsdb_get_cacheblock(xdag_hashlow_t hash, struct xdag_block *xb);

xd_rsdb_op_t xdag_rsdb_putkey(const char* key, size_t klen, const char* value, size_t vlen);
xd_rsdb_op_t xdag_rsdb_put_setting(xd_rsdb_key_t type, const char* value, size_t vlen);
xd_rsdb_op_t xdag_rsdb_put_bi(struct block_internal* bi);
xd_rsdb_op_t xdag_rsdb_put_ourbi(struct block_internal* bi);
xd_rsdb_op_t xdag_rsdb_put_orpblock(xdag_hashlow_t hash, struct xdag_block* xb);
xd_rsdb_op_t xdag_rsdb_put_stats(void);
xd_rsdb_op_t xdag_rsdb_put_extstats(void);
xd_rsdb_op_t xdag_rsdb_put_remark(struct block_internal *bi, xdag_remark_t strbuf);
xd_rsdb_op_t xdag_rsdb_put_syncblock(xdag_hashlow_t ref_hash, xdag_hashlow_t block_hash, struct sync_block *sb);
xd_rsdb_op_t xdag_rsdb_put_cacheblock(xdag_hashlow_t hash, struct xdag_block *xb);

xd_rsdb_op_t xdag_rsdb_delkey(const char* key, size_t klen);
xd_rsdb_op_t xdag_rsdb_del_orpblock(xdag_hashlow_t hash);
xd_rsdb_op_t xdag_rsdb_del_syncblock(xdag_hashlow_t ref_hash, xdag_hashlow_t block_hash);


#endif
