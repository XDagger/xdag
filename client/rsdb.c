#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // sysconf() - get CPU count
#include "rsdb.h"
#include "global.h"
#include "utils/log.h"
#include "version.h"

#define RSDB_LOG_FREE_ERRMSG(errmsg) xdag_info("%s %lu %s \n",__FUNCTION__, __LINE__,errmsg);free(errmsg)

char* xd_rsdb_full_merge(void* state, const char* key, size_t key_length,
                         const char* existing_value,
                         size_t existing_value_length,
                         const char* const* operands_list,
                         const size_t* operands_list_length, int num_operands,
                         unsigned char* success, size_t* new_value_length)
{
    struct block_internal *result = calloc(sizeof(struct block_internal), 1);
    for(int i = 0; i < num_operands; i++) {
        struct block_internal *bi = (struct block_internal*)operands_list[i];
        memcpy(result, bi, sizeof(struct block_internal));
    }

    if (!result) {
        *success = 0;
        *new_value_length = 0;
        return NULL;
    }
    *success = 1;
    *new_value_length = sizeof(struct block_internal) ;
    return (char*)result;
}

char* xd_rsdb_partial_merge(void* state, const char* key, size_t key_length,
                       const char* const* operands_list,
                       const size_t* operands_list_length, int num_operands,
                       unsigned char* success, size_t* new_value_length)
{
    struct block_internal *result = calloc(sizeof(struct block_internal), 1);
    for(int i = 0; i < num_operands; i++) {
        struct block_internal *bi = (struct block_internal*)operands_list[i];
        memcpy(result, bi, sizeof(struct block_internal));
    }

    if (!result) {
        *success = 0;
        *new_value_length = 0;
        return NULL;
    }
    *success = 1;
    *new_value_length = sizeof(struct block_internal) ;
    return (char*)result;
}

const char *xd_rsdb_merge_operator_name(void* test)
{
    return "xd_rsdb_merge_operator";
}

xd_rsdb_op_t xd_rsdb_pre_init(int read_only)
{
    xd_rsdb_t* rsdb = NULL;
    xd_rsdb_conf_t* rsdb_config = NULL;

    rsdb = malloc(sizeof(xd_rsdb_t));
    rsdb_config = malloc(sizeof(xd_rsdb_conf_t));
    memset(rsdb, 0, sizeof(xd_rsdb_t));
    memset(rsdb_config, 0, sizeof(xd_rsdb_conf_t));

    rsdb->config = rsdb_config;
    rsdb->config->db_name = strdup(g_xdag_testnet?"chainstate-test":"chainstate");
    rsdb->config->db_path = strdup(g_xdag_testnet?"chainstate-test":"chainstate");

    int error_code = 0;
    if((error_code = xd_rsdb_conf(rsdb))) {
        return error_code;
    }

    if((error_code = xd_rsdb_open(rsdb, read_only))) {
        return error_code;
    }
    g_xdag_rsdb = rsdb;
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_init(xdag_time_t *time)
{
    char key[1] = {[0]=SETTING_CREATED};
    char* value = NULL;
    size_t vlen = 0;
    value = xd_rsdb_getkey(key, 1, &vlen);
    if(!value) {
        xd_rsdb_put_setting(SETTING_VERSION, XDAG_VERSION, strlen(XDAG_VERSION));
        xd_rsdb_put_setting(SETTING_CREATED, "done", strlen("done"));
    } else if (strncmp("done", value, strlen("done")) == 0) {
        free(value);
        value = NULL;
        xd_rsdb_load(g_xdag_rsdb);
        char time_key[1] = {[0]=SETTING_CUR_TIME};
        if((value = xd_rsdb_getkey(time_key, 1, &vlen))) {
            memcpy(time, value, sizeof(xdag_time_t));
            free(value);
        }
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_load(xd_rsdb_t* db)
{
    xd_rsdb_get_stats();
    xd_rsdb_get_extstats();

    // clean wait blocks
    g_xdag_extstats.nwaitsync = 0;

    // read top_main_chain hash
    char top_main_chain_hash_key[1] ={[0]=SETTING_TOP_MAIN_HASH};
    char *value = NULL;
    size_t vlen = 0;

    value = xd_rsdb_getkey(top_main_chain_hash_key, 1, &vlen);
    if(value)
    {
        memcpy(g_top_main_chain_hash, value, sizeof(g_top_main_chain_hash));
        free(value);
        value = NULL;
    }

    char pre_top_main_chain_hash_key[1] ={[0]=SETTING_PRE_TOP_MAIN_HASH};
    value = xd_rsdb_getkey(pre_top_main_chain_hash_key, 1, &vlen);
    if(value)
    {
        memcpy(g_pre_top_main_chain_hash, value, sizeof(g_pre_top_main_chain_hash));
        free(value);
        value = NULL;
    }

    char ourfirst_hash_key[1] ={[0]=SETTING_OUR_FIRST_HASH};
    value = xd_rsdb_getkey(ourfirst_hash_key, 1, &vlen);
    if(value)
    {
        memcpy(g_ourfirst_hash, value, sizeof(g_ourfirst_hash));
        free(value);
        value = NULL;
    }

    char ourlast_hash_key[1] ={[0]=SETTING_OUR_LAST_HASH};
    value = xd_rsdb_getkey(ourlast_hash_key, 1, &vlen);
    if(value)
    {
        memcpy(g_ourlast_hash, value, sizeof(g_ourlast_hash));
        free(value);
        value = NULL;
    }

    char balance_key[1] ={[0]=SETTING_OUR_BALANCE};
    value = xd_rsdb_getkey(balance_key, 1, &vlen);
    if(value)
    {
        memcpy(&g_balance, value, sizeof(g_balance));
        free(value);
        value = NULL;
    }

    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_conf_check(xd_rsdb_t  *db)
{
    if(db == NULL ||
       !strlen(db->config->db_name) ||
       !strlen(db->config->db_path)
            )
    {
        return XDAG_RSDB_CONF_ERROR;
    }
    return  XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_conf(xd_rsdb_t  *db)
{
    int error_code = 0;
    if( (error_code = xd_rsdb_conf_check(db)) ) {
        return error_code;
    }
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);  // get # of online cores

    rocksdb_options_t* options = rocksdb_options_create();
    rocksdb_writeoptions_t* write_options = rocksdb_writeoptions_create();
    rocksdb_readoptions_t* read_options = rocksdb_readoptions_create();
    rocksdb_filterpolicy_t *filter_policy = rocksdb_filterpolicy_create_bloom_full(10);
    struct merge_operator_state *state = malloc(sizeof(*state));
    rocksdb_mergeoperator_t *merge_operator = rocksdb_mergeoperator_create((void *)state,
                                                                           NULL,
                                                                           xd_rsdb_full_merge,
                                                                           xd_rsdb_partial_merge,
                                                                           NULL,
                                                                           xd_rsdb_merge_operator_name);


    // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
    rocksdb_options_increase_parallelism(options, (int)(cpus));

    // set max open file limit
    rocksdb_options_set_max_open_files(options, 1024);
    rocksdb_options_set_create_if_missing(options, 1);
    rocksdb_options_set_compression(options, rocksdb_lz4_compression);

    rocksdb_block_based_table_options_t *block_based_table_options = rocksdb_block_based_options_create();
    rocksdb_block_based_options_set_filter_policy(block_based_table_options, filter_policy);
    rocksdb_options_set_block_based_table_factory(options, block_based_table_options);
    rocksdb_options_set_merge_operator(options, merge_operator);

    db->options = options;
    db->write_options = write_options;
    db->read_options = read_options;
    db->merge_operator = merge_operator;
    db->filter_policy = filter_policy;
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_open(xd_rsdb_t* db, int read_only)
{
    char *errmsg = NULL;
    if(read_only) {
        db->db = rocksdb_open_for_read_only(db->options, db->config->db_path, (unsigned char)NULL, &errmsg);
    } else {
        db->db = rocksdb_open(db->options, db->config->db_path, &errmsg);
    }

    if(errmsg)
    {
		RSDB_LOG_FREE_ERRMSG(errmsg);
        return XDAG_RSDB_OPEN_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_close(xd_rsdb_t* db)
{
    if(db->write_options) rocksdb_writeoptions_destroy(db->write_options);
    if(db->read_options) rocksdb_readoptions_destroy(db->read_options);
    if(db->options) rocksdb_options_destroy(db->options);
    if(db->db) rocksdb_close(db->db);
    return XDAG_RSDB_OP_SUCCESS;
}

// get
void* xd_rsdb_getkey(const char* key, const size_t klen, size_t* vlen)
{
    char *errmsg = NULL;
    char *rocksdb_return_value = rocksdb_get(g_xdag_rsdb->db, g_xdag_rsdb->read_options, key, klen, vlen, &errmsg);

    if(errmsg)
    {
        if(rocksdb_return_value) {
            free(rocksdb_return_value);
        }
        rocksdb_readoptions_destroy(g_xdag_rsdb->read_options);
        rocksdb_close(g_xdag_rsdb->db);
		RSDB_LOG_FREE_ERRMSG(errmsg);
        return NULL;
    }
    return rocksdb_return_value;
}

xd_rsdb_op_t xd_rsdb_get_bi(xdag_hashlow_t hash, struct block_internal *bi)
{
	if (!hash || !bi) return XDAG_RSDB_NULL;
	size_t vlen = 0;
	char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_INTERNAL};
	memcpy(key + 1, hash, RSDB_KEY_LEN - 1 );
	char *value = xd_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);
	if(!value)
		return XDAG_RSDB_NULL;
	
	if(vlen != sizeof(struct block_internal)){
		xdag_err("vlen is not math size of block_internal\n");
		free(value);
		return XDAG_RSDB_NULL;
	}
	memcpy(bi,value,vlen);
	free(value);

	return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_get_ournext(xdag_hashlow_t hash, xdag_hashlow_t next)
{
    if (!hash) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_OUR};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1 );
    char *value = xd_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);
    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(xdag_hashlow_t)){
        xdag_err("vlen is not math size of xdag_hashlow_t\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(next, value, vlen);
    free(value);

    return XDAG_RSDB_OP_SUCCESS;
}

static xd_rsdb_op_t xd_rsdb_get_xdblock(xdag_hashlow_t hash, xd_rsdb_key_t type, struct xdag_block *xb)
{
    if(!xb) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = type};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xd_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(struct xdag_block)){
        fprintf(stderr,"vlen is not math size of xdag_block\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(xb, value, vlen);
    free(value);
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_get_orpblock(xdag_hashlow_t hash, struct xdag_block *xb)
{
    return xd_rsdb_get_xdblock(hash, HASH_ORP_BLOCK, xb);
}

xd_rsdb_op_t xd_rsdb_get_extblock(xdag_hashlow_t hash, struct xdag_block *xb)
{
    return xd_rsdb_get_xdblock(hash, HASH_EXT_BLOCK, xb);
}

xd_rsdb_op_t xd_rsdb_get_cacheblock(xdag_hashlow_t hash, struct xdag_block *xb)
{
    return xd_rsdb_get_xdblock(hash, HASH_BLOCK_CACHE, xb);;
}

xd_rsdb_op_t xd_rsdb_get_stats(void)
{
    char key[1] = {[0] = SETTING_STATS};
    struct xdag_stats* p = NULL;
    size_t vlen = 0;
    p = xd_rsdb_getkey(key, 1, &vlen);
    if(p) {
        memcpy(&g_xdag_stats, p, sizeof(g_xdag_stats));
        free(p);
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_get_extstats()
{
    char key[1] = {[0] = SETTING_EXT_STATS};
    struct xdag_ext_stats* pexs = NULL;
    size_t vlen = 0;
    pexs = xd_rsdb_getkey(key, 1, &vlen);
    if(pexs) {
        memcpy(&g_xdag_extstats, pexs, sizeof(g_xdag_extstats));
        free(pexs);
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_get_remark(xdag_hashlow_t hash, xdag_remark_t remark)
{
    if(!remark) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_REMARK};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xd_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(xdag_remark_t)){
        fprintf(stderr,"vlen is not math size of xdag_remark_t\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(remark, value, vlen);
    free(value);
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_get_heighthash(uint64_t height, xdag_hashlow_t hash)
{
    if (!hash) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[sizeof(uint64_t) + 1] = {[0] = HEIGHT_BLOCK_HASH};
    memcpy(key + 1, &height, sizeof(uint64_t));
    char *value = xd_rsdb_getkey(key, sizeof(uint64_t) + 1, &vlen);
    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(xdag_hashlow_t)){
        xdag_err("vlen is not math size of xdag_hashlow_t\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(hash, value, vlen);
    free(value);

    return XDAG_RSDB_OP_SUCCESS;
}

// put
xd_rsdb_op_t xd_rsdb_putkey(const char* key, size_t klen, const char* value, size_t vlen)
{
    char *errmsg = NULL;

    rocksdb_put(g_xdag_rsdb->db,
                g_xdag_rsdb->write_options,
                key, klen,
                value, vlen,
                &errmsg);
    if(errmsg)
    {
        rocksdb_writeoptions_destroy(g_xdag_rsdb->write_options);
        rocksdb_close(g_xdag_rsdb->db);
        RSDB_LOG_FREE_ERRMSG(errmsg);
        return XDAG_RSDB_PUT_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_put_backref(xdag_hashlow_t backref, struct block_internal* bi)
{
    int retcode = 0;
    if(!backref) return XDAG_RSDB_NULL;
    char key[RSDB_KEY_LEN * 2] = {[0] = HASH_BLOCK_BACKREF};
    memcpy(key + 1, backref, RSDB_KEY_LEN - 1);
    key[RSDB_KEY_LEN] = '_';
    memcpy(key + RSDB_KEY_LEN + 1, bi->hash, RSDB_KEY_LEN - 1);
    retcode = xd_rsdb_putkey(key, RSDB_KEY_LEN * 2, (const char *) bi->hash, sizeof(xdag_hashlow_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_put_ournext(xdag_hashlow_t hash, xdag_hashlow_t next)
{
    int retcode = 0;
    if(!hash) return XDAG_RSDB_NULL;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_OUR};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xd_rsdb_putkey(key, RSDB_KEY_LEN, (const char *)next, sizeof(xdag_hashlow_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_put_setting(xd_rsdb_key_t type, const char* value, size_t vlen)
{
    char key[1] = {[0] = type};
    xd_rsdb_putkey(key, 1, value, vlen);
    return XDAG_RSDB_OP_SUCCESS;
}

static xd_rsdb_op_t xd_rsdb_put_xdblock(xdag_hashlow_t hash, xd_rsdb_key_t type,struct xdag_block* xb)
{
    if(!hash) return XDAG_RSDB_NULL;
    if(!xb) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = type};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xd_rsdb_putkey(key, RSDB_KEY_LEN, (const char *) xb, sizeof(struct xdag_block));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_put_orpblock(xdag_hashlow_t hash, struct xdag_block* xb)
{
    return xd_rsdb_put_xdblock(hash, HASH_ORP_BLOCK, xb);
}

xd_rsdb_op_t xd_rsdb_put_extblock(xdag_hashlow_t hash, struct xdag_block* xb)
{
    return xd_rsdb_put_xdblock(hash, HASH_EXT_BLOCK, xb);
}

xd_rsdb_op_t xd_rsdb_put_cacheblock(xdag_hashlow_t hash, struct xdag_block* xb)
{
    return xd_rsdb_put_xdblock(hash, HASH_BLOCK_CACHE, xb);;
}

xd_rsdb_op_t xd_rsdb_put_stats(xdag_time_t time)
{
    xd_rsdb_op_t ret = XDAG_RSDB_NULL;
    if((ret = xd_rsdb_put_setting(SETTING_STATS, (const char *) &g_xdag_stats, sizeof(g_xdag_stats)))) {
        return ret;
    }
    ret = xd_rsdb_put_setting(SETTING_CUR_TIME, (const char*)&time, sizeof(xdag_time_t));
    return ret;
}

xd_rsdb_op_t xd_rsdb_put_extstats(void)
{
    return xd_rsdb_put_setting(SETTING_EXT_STATS, (const char *) &g_xdag_extstats, sizeof(g_xdag_extstats));;
}

xd_rsdb_op_t xd_rsdb_put_remark(struct block_internal* bi, xdag_remark_t strbuf)
{
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_REMARK};
    memcpy(key + 1, bi->hash, RSDB_KEY_LEN - 1);
    retcode = xd_rsdb_putkey(key, RSDB_KEY_LEN, (const char *) strbuf, sizeof(xdag_remark_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_put_heighthash(uint64_t height, xdag_hashlow_t hash)
{
    int retcode = 0;
    if(!hash) return XDAG_RSDB_NULL;
    char key[sizeof(uint64_t) + 1] = {[0] = HEIGHT_BLOCK_HASH};
    memcpy(key + 1, &height, sizeof(uint64_t));
    retcode = xd_rsdb_putkey(key, sizeof(uint64_t) + 1, (const char *)hash, sizeof(xdag_hashlow_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

// delete
xd_rsdb_op_t xd_rsdb_delkey(const char* key, size_t klen)
{
    char *errmsg = NULL;
    rocksdb_delete(g_xdag_rsdb->db, g_xdag_rsdb->write_options, key, klen, &errmsg);
    if(errmsg)
    {
        rocksdb_writeoptions_destroy(g_xdag_rsdb->write_options);
        rocksdb_close(g_xdag_rsdb->db);
        RSDB_LOG_FREE_ERRMSG(errmsg);
        return XDAG_RSDB_DELETE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

static xd_rsdb_op_t xd_rsdb_del(xdag_hashlow_t hash, xd_rsdb_key_t type)
{
    if(!hash) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = type};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xd_rsdb_delkey(key, RSDB_KEY_LEN);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_del_bi(xdag_hashlow_t hash)
{
    return xd_rsdb_del(hash, HASH_BLOCK_INTERNAL);
}

xd_rsdb_op_t xd_rsdb_del_orpblock(xdag_hashlow_t hash)
{
    return xd_rsdb_del(hash, HASH_ORP_BLOCK);
}

xd_rsdb_op_t xd_rsdb_del_extblock(xdag_hashlow_t hash)
{
    return xd_rsdb_del(hash, HASH_EXT_BLOCK);
}

xd_rsdb_op_t xd_rsdb_del_heighthash(uint64_t height)
{
    int retcode = 0;
    char key[sizeof(uint64_t) + 1] = {[0] = HEIGHT_BLOCK_HASH};
    memcpy(key + 1, &height, sizeof(uint64_t));
    retcode = xd_rsdb_delkey(key, sizeof(uint64_t) + 1);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

xd_rsdb_op_t xd_rsdb_merge_bi(struct block_internal* bi)
{
    char *errmsg = NULL;
    if(!bi) return XDAG_RSDB_NULL;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, RSDB_KEY_LEN - 1);

    rocksdb_merge(g_xdag_rsdb->db,
                  g_xdag_rsdb->write_options,
                  key, RSDB_KEY_LEN,
                  (const char*)bi, sizeof(struct block_internal),
                  &errmsg);
    if(errmsg)
    {
        rocksdb_mergeoperator_destroy(g_xdag_rsdb->merge_operator);
        rocksdb_close(g_xdag_rsdb->db);
        RSDB_LOG_FREE_ERRMSG(errmsg);
        return XDAG_RSDB_MERGE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}