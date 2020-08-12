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

struct merge_operator_state {
    int val;
};

typedef struct xdag_rsdb_conf {
    char                              *db_name;
    char                              *db_path;
} xd_rsdb_conf_t;

typedef struct xdag_rsdb {
    xd_rsdb_conf_t                    *config;
    rocksdb_options_t                *options;
    rocksdb_readoptions_t       *read_options;
    rocksdb_writeoptions_t     *write_options;
    rocksdb_mergeoperator_t   *merge_operator;
    rocksdb_filterpolicy_t     *filter_policy;
    rocksdb_t                             *db;
} xd_rsdb_t;

typedef enum xd_rsdb_op_result {
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
    XDAG_RSDB_MERGE_ERROR                 = 14,
    XDAG_RSDB_SEEK_ERROR                  = 15
} xd_rsdb_op_t;

typedef enum xd_rsdb_key_type {
    SETTING_VERSION                       =  0x10,
    SETTING_CREATED                       =  0x11,
    SETTING_STATS                         =  0x12,
    SETTING_EXT_STATS                     =  0x13,
    SETTING_PRE_TOP_MAIN_HASH             =  0x14,
    SETTING_TOP_MAIN_HASH                 =  0x15,
    SETTING_OUR_FIRST_HASH                =  0x16,
    SETTING_OUR_LAST_HASH                 =  0x17,
    SETTING_OUR_BALANCE                   =  0x18,
    SETTING_CUR_TIME                      =  0x19,
    HASH_ORP_BLOCK                        =  0x20,
    HASH_EXT_BLOCK                        =  0x21,
    HASH_BLOCK_INTERNAL                   =  0x22,
    HASH_BLOCK_OUR                        =  0x23,
    HASH_BLOCK_REMARK                     =  0x24,
    HASH_BLOCK_BACKREF                    =  0x25,
    HASH_BLOCK_CACHE                      =  0x26,
    HEIGHT_BLOCK_HASH                     =  0x27
} xd_rsdb_key_t;

char* xd_rsdb_full_merge(void* state, const char* key, size_t key_length,
                         const char* existing_value,
                         size_t existing_value_length,
                         const char* const* operands_list,
                         const size_t* operands_list_length, int num_operands,
                         unsigned char* success, size_t* new_value_length);
char* xd_rsdb_partial_merge(void*, const char* key, size_t key_length,
                            const char* const* operands_list,
                            const size_t* operands_list_length, int num_operands,
                            unsigned char* success, size_t* new_value_length);
const char* xd_rsdb_merge_operator_name(void*);

xd_rsdb_op_t xd_rsdb_pre_init(int);
xd_rsdb_op_t xd_rsdb_init(xdag_time_t *time);
xd_rsdb_op_t xd_rsdb_load(xd_rsdb_t* db);
xd_rsdb_op_t xd_rsdb_conf_check(xd_rsdb_t  *db);
xd_rsdb_op_t xd_rsdb_conf(xd_rsdb_t* db);
xd_rsdb_op_t xd_rsdb_open(xd_rsdb_t* db, int);
xd_rsdb_op_t xd_rsdb_close(xd_rsdb_t* db);

//get
void* xd_rsdb_getkey(const char* key, const size_t klen, size_t* vlen);
xd_rsdb_op_t xd_rsdb_get_bi(xdag_hashlow_t hash, struct block_internal*);
xd_rsdb_op_t xd_rsdb_get_ournext(xdag_hashlow_t hash, xdag_hashlow_t next);
xd_rsdb_op_t xd_rsdb_get_orpblock(xdag_hashlow_t hash, struct xdag_block*);
xd_rsdb_op_t xd_rsdb_get_extblock(xdag_hashlow_t hash, struct xdag_block*);
xd_rsdb_op_t xd_rsdb_get_cacheblock(xdag_hashlow_t hash, struct xdag_block *xb);
xd_rsdb_op_t xd_rsdb_get_stats(void);
xd_rsdb_op_t xd_rsdb_get_extstats(void);
xd_rsdb_op_t xd_rsdb_get_remark(xdag_hashlow_t hash, xdag_remark_t);
xd_rsdb_op_t xd_rsdb_get_heighthash(uint64_t height, xdag_hashlow_t hash);

//put
xd_rsdb_op_t xd_rsdb_putkey(const char* key, size_t klen, const char* value, size_t vlen);
xd_rsdb_op_t xd_rsdb_put_backref(xdag_hashlow_t backref, struct block_internal*);
xd_rsdb_op_t xd_rsdb_put_ournext(xdag_hashlow_t hash, xdag_hashlow_t next);
xd_rsdb_op_t xd_rsdb_put_setting(xd_rsdb_key_t type, const char* value, size_t vlen);
xd_rsdb_op_t xd_rsdb_put_orpblock(xdag_hashlow_t hash, struct xdag_block* xb);
xd_rsdb_op_t xd_rsdb_put_extblock(xdag_hashlow_t hash, struct xdag_block* xb);
xd_rsdb_op_t xd_rsdb_put_stats(xdag_time_t time);
xd_rsdb_op_t xd_rsdb_put_extstats(void);
xd_rsdb_op_t xd_rsdb_put_remark(struct block_internal *bi, xdag_remark_t strbuf);
xd_rsdb_op_t xd_rsdb_put_cacheblock(xdag_hashlow_t hash, struct xdag_block *xb);
xd_rsdb_op_t xd_rsdb_put_heighthash(uint64_t height, xdag_hashlow_t hash);

//del
xd_rsdb_op_t xd_rsdb_delkey(const char* key, size_t klen);
xd_rsdb_op_t xd_rsdb_del_bi(xdag_hashlow_t hash);
xd_rsdb_op_t xd_rsdb_del_orpblock(xdag_hashlow_t hash);
xd_rsdb_op_t xd_rsdb_del_extblock(xdag_hashlow_t hash);
xd_rsdb_op_t xd_rsdb_del_heighthash(uint64_t height);
xd_rsdb_op_t xd_rsdb_merge_bi(struct block_internal* bi);

#endif
