#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // sysconf() - get CPU count
#include "rsdb.h"
#include "global.h"

XDAG_RSDB_OP_TYPE xdag_rsdb_pre_init(void)
{
    XDAG_RSDB* rsdb = NULL;
    XDAG_RSDB_CONF* rsdb_config = NULL;

    rsdb = malloc(sizeof(XDAG_RSDB));
    rsdb_config = malloc(sizeof(XDAG_RSDB_CONF));
    memset(rsdb, 0, sizeof(XDAG_RSDB));
    memset(rsdb_config, 0, sizeof(XDAG_RSDB_CONF));

    rsdb->config = rsdb_config;
    rsdb->config->db_name = strdup("chainstate");
    rsdb->config->db_path = strdup("chainstate");

    int error_code = 0;
    if((error_code = xdag_rsdb_conf(rsdb))) {
        return error_code;
    }

    if((error_code = xdag_rsdb_open(rsdb))) {
        return error_code;
    }
    g_xdag_rsdb = rsdb;
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_init(void)
{
    char key[1] ={[0]=SETTING_CREATED};
    char* value = NULL;
    size_t vlen = 0;
    value = xdag_rsdb_getkey(key, 1, &vlen);
    if(!value) {
        xdag_rsdb_put_setting(SETTING_CREATED, "done", strlen("done"));
        return XDAG_RSDB_INIT_NEW;
    } else if (strncmp("done", value, strlen("done")) == 0) {
        free(value);
        xdag_rsdb_load(g_xdag_rsdb);
        return XDAG_RSDB_INIT_LOAD;
    }
    return XDAG_RSDB_NULL;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_conf_check(XDAG_RSDB  *rsdb)
{
    if(rsdb == NULL ||
       !strlen(rsdb->config->db_name) ||
       !strlen(rsdb->config->db_path)
    )
    {
        return XDAG_RSDB_CONF_ERROR;
    }
    return  XDAG_RSDB_OP_SUCCESS;
}


XDAG_RSDB_OP_TYPE xdag_rsdb_conf(XDAG_RSDB  *rsdb)
{
    char* errmsg = NULL;
    int error_code = 0;
    if( (error_code = xdag_rsdb_conf_check(rsdb)) ) {
        return error_code;
    }
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);  // get # of online cores
    rocksdb_options_t* options = rocksdb_options_create();
    rocksdb_writeoptions_t* write_options = rocksdb_writeoptions_create();
    rocksdb_readoptions_t* read_options = rocksdb_readoptions_create();
    rocksdb_flushoptions_t* flush_options = rocksdb_flushoptions_create();
    rocksdb_restore_options_t* restore_options = rocksdb_restore_options_create();

    // set max open file limit
    rocksdb_options_set_max_open_files(options, 1024);
    rocksdb_options_set_create_if_missing(options, 1);
    
    rocksdb_options_set_compression(options, rocksdb_lz4_compression);
    rocksdb_options_increase_parallelism(options, (int)(cpus));
    
//    rocksdb_block_based_table_options_t* block_based_table_options = rocksdb_block_based_options_create();
//    rocksdb_block_based_options_set_block_size(block_based_table_options, 16 * 1024);
//    rocksdb_cache_t* block_cache = rocksdb_cache_create_lru(32 * 1024 * 1024);
//    rocksdb_block_based_options_set_block_cache(block_based_table_options, block_cache);
//    rocksdb_block_based_options_set_cache_index_and_filter_blocks(block_based_table_options, 1);
//    rocksdb_block_based_options_set_pin_l0_filter_and_index_blocks_in_cache(block_based_table_options, 1);

    // Optimize RocksDB. This is the easiest way to
    // get RocksDB to perform well
//    rocksdb_options_increase_parallelism(options, (int)(cpus));
//    rocksdb_readoptions_set_prefix_same_as_start(read_options, 1);
//    rocksdb_readoptions_set_verify_checksums(read_options, 0);

    // create the DB if it's not already present
//    rocksdb_options_set_block_based_table_factory(options, block_based_table_options);
    
    rsdb->options = options;
    rsdb->write_options = write_options;
    rsdb->read_options = read_options;

    if(errmsg)
    {
        return XDAG_RSDB_BKUP_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_open(XDAG_RSDB* rsdb)
{
    char *errmsg = NULL;
    
    rsdb->db = rocksdb_open(rsdb->options, rsdb->config->db_path, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_OPEN_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_close(XDAG_RSDB* rsdb)
{
    if(rsdb->write_options) rocksdb_writeoptions_destroy(rsdb->write_options);
    if(rsdb->read_options) rocksdb_readoptions_destroy(rsdb->read_options);
    if(rsdb->options) rocksdb_options_destroy(rsdb->options);
    if(rsdb->db) rocksdb_close(rsdb->db);
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_load(XDAG_RSDB* db)
{
    xdag_rsdb_get_stats();
    xdag_rsdb_get_extstats();

    // clean wait blocks
    g_xdag_extstats.nwaitsync = 0;

    // read top_main_chain hash
    char key[1] ={[0]=SETTING_TOP_MAIN_HASH};
    char *value = NULL;
    size_t vlen = 0;

    value = xdag_rsdb_getkey(key, 1, &vlen);
    if(value)
    {
        memcpy(g_top_main_chain_hash, value, sizeof(g_top_main_chain_hash));
        free(value);
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_putkey(const char* key, const size_t klen, const char* value, size_t vlen)
{
    char *errmsg = NULL;
    
    rocksdb_put(g_xdag_rsdb->db,
                g_xdag_rsdb->write_options,
                key, klen,
                value, vlen,
                &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_PUT_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_setting(XDAG_RSDB_KEY_TYPE type, const char* value, size_t vlen)
{
    char key[1] = {[0] = type};
    xdag_rsdb_putkey(key, 1, value, vlen);
    return XDAG_RSDB_OP_SUCCESS;
}

void* xdag_rsdb_getkey(const char* key, size_t klen, size_t* vlen)
{
    char *errmsg = NULL;
    char *rocksdb_return_value = rocksdb_get(g_xdag_rsdb->db, g_xdag_rsdb->read_options, key, klen, vlen, &errmsg);

    if(errmsg)
    {
        if(rocksdb_return_value) {
            free(rocksdb_return_value);
        }
        return NULL;
    }
    return rocksdb_return_value;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_delkey(const char* key, const size_t klen)
{
    char *errmsg = NULL;
    rocksdb_delete(g_xdag_rsdb->db, g_xdag_rsdb->write_options, key, klen, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_DELETE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}


XDAG_RSDB_OP_TYPE xdag_rsdb_put_stats(void)
{
    return xdag_rsdb_put_setting(SETTING_STATS, (const char*)&g_xdag_stats, sizeof(g_xdag_stats));
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_stats(void)
{
    char key[1] = {[0] = SETTING_STATS};
    struct xdag_stats* p = NULL;
    size_t vlen = 0;
    p = xdag_rsdb_getkey(key, 1, &vlen);
    if(p) {
        memcpy(&g_xdag_stats, p, sizeof(g_xdag_stats));
        free(p);
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_extstats()
{
    char key[1] = {[0] = SETTING_EXT_STATS};
    struct xdag_ext_stats* pexs = NULL;
    size_t vlen = 0;
    pexs = xdag_rsdb_getkey(key, 1, &vlen);
    if(pexs) {
        memcpy(&g_xdag_extstats, pexs, sizeof(g_xdag_extstats));
        free(pexs);
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_extstats(void)
{
    return xdag_rsdb_put_setting(SETTING_EXT_STATS, (const char*)&g_xdag_extstats, sizeof(g_xdag_extstats));;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_bi(xdag_hashlow_t hash,struct block_internal* bi)
{
	if (!hash || !bi) return XDAG_RSDB_NULL;
	
	if(hash[0] == 0 && hash[1] == 0 && hash[2] == 0) return XDAG_RSDB_NULL;
	size_t vlen = 0;
	char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_INTERNAL};
	memcpy(key + 1, hash, RSDB_KEY_LEN - 1 );
	char *value = xdag_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);
	if(!value)
		return XDAG_RSDB_NULL;
	
	if(vlen != sizeof(struct block_internal)){
		fprintf(stderr,"vlen is not math size of block_internal\n");
		free(value);
		return XDAG_RSDB_NULL;
	}
	memcpy(bi,value,vlen);
	free(value);

	return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_seek_orpblock(struct xdag_block *xb)
{
    if(!xb) return XDAG_RSDB_NULL;
    char key[1] = {[0] = HASH_ORP_BLOCK};
    size_t vlen = 0;
    rocksdb_iterator_t* iter = NULL;

    iter = rocksdb_create_iterator(g_xdag_rsdb->db, g_xdag_rsdb->read_options);
    rocksdb_iter_seek(iter, key, 1);
    if(!rocksdb_iter_valid(iter)) {
       rocksdb_iter_destroy(iter);
       return XDAG_RSDB_NULL;
    }
    const char *value = rocksdb_iter_value(iter, &vlen);
    if(value) {
        xb = malloc(sizeof(struct xdag_block));
        memset(xb, 0, sizeof(struct xdag_block));
        memcpy(xb, value, sizeof(struct xdag_block));
    }
    rocksdb_iter_destroy(iter);
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_orpblock(xdag_hashlow_t hash, struct xdag_block *xb)
{
    if(!xb) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_ORP_BLOCK};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xdag_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

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

XDAG_RSDB_OP_TYPE xdag_rsdb_get_syncblock(xdag_hashlow_t hash, struct sync_block *sb)
{
    if(!sb) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_SYNC};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xdag_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(struct sync_block)){
        fprintf(stderr,"vlen is not math size of sync_block\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(sb, value, vlen);
    free(value);
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_del_orpblock(xdag_hashlow_t hash)
{
    if(!hash) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_ORP_BLOCK};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_delkey(key, RSDB_KEY_LEN);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_del_syncblock(xdag_hashlow_t hash)
{
    if(!hash) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_SYNC};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_delkey(key, RSDB_KEY_LEN);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_ourbi(xdag_hashlow_t hash, struct block_internal *bi)
{
    if(!bi) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_OUR_BLOCK_INTERNAL};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xdag_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

    if(!value)
        return XDAG_RSDB_NULL;

    if(vlen != sizeof(struct block_internal)){
        fprintf(stderr,"vlen is not math size of block_internal\n");
        free(value);
        return XDAG_RSDB_NULL;
    }
    memcpy(bi, value, vlen);
    free(value);
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_get_remark(xdag_hashlow_t hash, xdag_remark_t remark)
{
    if(!remark) return XDAG_RSDB_NULL;
    size_t vlen = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_REMARK};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    char *value = xdag_rsdb_getkey(key, RSDB_KEY_LEN, &vlen);

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

XDAG_RSDB_OP_TYPE xdag_rsdb_put_bi(struct block_internal* bi)
{
    if(!bi) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_putkey(key, RSDB_KEY_LEN, (const char*)bi, sizeof(struct block_internal));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_orpblock(xdag_hashlow_t hash, struct xdag_block* xb)
{
    if(!hash) return XDAG_RSDB_NULL;
    if(!xb) return XDAG_RSDB_NULL;
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_ORP_BLOCK};
    memcpy(key + 1, hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_putkey(key, RSDB_KEY_LEN, (const char*)xb, sizeof(struct xdag_block));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_ourbi(struct block_internal* bi)
{
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_OUR_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_putkey(key, RSDB_KEY_LEN, (const char*)bi, sizeof(struct block_internal));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_remark(struct block_internal* bi, xdag_remark_t strbuf)
{
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_REMARK};
    memcpy(key + 1, bi->hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_putkey(key, RSDB_KEY_LEN, (const char*)strbuf, sizeof(xdag_remark_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_OP_TYPE xdag_rsdb_put_syncblock(struct sync_block *sb)
{
    int retcode = 0;
    char key[RSDB_KEY_LEN] = {[0] = HASH_BLOCK_SYNC};
    memcpy(key + 1, sb->hash, RSDB_KEY_LEN - 1);
    retcode = xdag_rsdb_putkey(key, RSDB_KEY_LEN, (const char*)sb, sizeof(struct sync_block));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}
