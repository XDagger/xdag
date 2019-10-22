#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // sysconf() - get CPU count
#include <pthread.h>

#include "rsdb.h"
#include "global.h"

int xdag_rsdb_conf_check(XDAG_RSDB  *rsdb)
{
    if(rsdb == NULL ||
       !strlen(rsdb->config->db_name) ||
       !strlen(rsdb->config->db_path) ||
       !strlen(rsdb->config->db_backup_path)
    )
    {
        return XDAG_RSDB_CONF_ERROR;
    }
    return  XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_check(XDAG_RSDB  *rsdb)
{
    if(xdag_rsdb_conf_check(rsdb))
    {
        return XDAG_RSDB_CONF_ERROR;
    }
    if(!rsdb->db) {
        return XDAG_RSDB_NULL;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_conf(XDAG_RSDB  *rsdb)
{
    char* errmsg = NULL;
    int error_code = 0;
    if( (error_code = xdag_rsdb_conf_check(rsdb)) ) {
        return error_code;
    }
    rocksdb_options_t* options = rocksdb_options_create();
    rocksdb_writeoptions_t* write_options = rocksdb_writeoptions_create();
    rocksdb_readoptions_t* read_options = rocksdb_readoptions_create();
    rocksdb_flushoptions_t* flush_options = rocksdb_flushoptions_create();
    rocksdb_restore_options_t* restore_options = rocksdb_restore_options_create();
    
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);  // get # of online cores
    // Optimize RocksDB. This is the easiest way to
    // get RocksDB to perform well
    rocksdb_options_increase_parallelism(options, (int)(cpus));
    rocksdb_options_optimize_level_style_compaction(options, 0);
    
    // create the DB if it's not already present
    rocksdb_options_set_create_if_missing(options, 1);
    
    rsdb->options = options;
    rsdb->write_options = write_options;
    rsdb->read_options = read_options;
    rsdb->flush_options = flush_options;
    rsdb->restore_options = restore_options;
    
    rsdb->backup_engine = rocksdb_backup_engine_open(rsdb->options, rsdb->config->db_backup_path, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_BKUP_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_open(XDAG_RSDB* rsdb)
{
    char *errmsg = NULL;
    
    rsdb->db = rocksdb_open(rsdb->options, rsdb->config->db_path, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_OPEN_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_backup(XDAG_RSDB  *rsdb)
{
    char *errmsg = NULL;
    rocksdb_backup_engine_create_new_backup(rsdb->backup_engine, rsdb->db, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_BKUP_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_restore(XDAG_RSDB* rsdb)
{
    char *errmsg = NULL;
    
    if(rsdb->db) rocksdb_close(rsdb->db);
    rocksdb_backup_engine_restore_db_from_latest_backup(rsdb->backup_engine,
                                                        rsdb->config->db_backup_path,
                                                        rsdb->config->db_backup_path,
                                                        rsdb->restore_options, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_BKUP_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_close(XDAG_RSDB* rsdb)
{
    if(rsdb->restore_options) rocksdb_restore_options_destroy(rsdb->restore_options);
    if(rsdb->write_options) rocksdb_writeoptions_destroy(rsdb->write_options);
    if(rsdb->read_options) rocksdb_readoptions_destroy(rsdb->read_options);
    if(rsdb->options) rocksdb_options_destroy(rsdb->options);
    if(rsdb->backup_engine) rocksdb_backup_engine_close(rsdb->backup_engine);
    if(rsdb->db) rocksdb_close(rsdb->db);
    return XDAG_RSDB_OP_SUCCESS;
}


XDAG_RSDB* xdag_rsdb_new(char* db_name, char* db_path, char* db_backup_path)
{
    XDAG_RSDB* rsdb = NULL;
    rsdb = calloc(sizeof(XDAG_RSDB), 1);
    rsdb->config = calloc(sizeof(XDAG_RSDB_CONF), 1);
    rsdb->config->db_name = strdup(db_name);
    rsdb->config->db_path = strdup(db_path);
    rsdb->config->db_backup_path = strdup(db_backup_path);
    return rsdb;
}

int xdag_rsdb_delete(XDAG_RSDB* rsdb)
{
    free(rsdb->config);
    free(rsdb);
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_load(XDAG_RSDB* db)
{
    char key[1] = {[0]=SETTING_STATS};
    size_t vlen = 0;
    xdag_rsdb_getkey(db, key, sizeof(key), (char*)&g_xdag_stats, &vlen);
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_putkey(XDAG_RSDB* rsdb, const char* key, size_t klen, const char* value, size_t vlen)
{
    char *errmsg = NULL;
    
    rocksdb_put(rsdb->db,
                rsdb->write_options,
                key, klen,
                value, vlen,
                &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_PUT_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_getkey(XDAG_RSDB* rsdb, const char* key, size_t klen, char* value, size_t* vlen)
{
    char *errmsg = NULL;

    char *return_value = rocksdb_get(rsdb->db, rsdb->read_options, key, klen, vlen, &errmsg);
    memcpy(value, return_value, *vlen);
    if(errmsg)
    {
        return XDAG_RSDB_GET_ERROR;
    }
    if(!return_value)
    {
        return XDAG_RSDB_KEY_NOT_EXIST;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_delkey(XDAG_RSDB* db, const char* key, size_t klen)
{
    char *errmsg = NULL;
    
    rocksdb_delete(db->db, db->write_options, key, klen, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_DELETE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

XDAG_RSDB_BATCH* xdag_rsdb_writebatch_new()
{
    XDAG_RSDB_BATCH* xbatch = calloc(sizeof(XDAG_RSDB_BATCH), 1);
    xbatch->writebatch = rocksdb_writebatch_create();
    return xbatch;
}

extern int xdag_rsdb_writebatch_put(XDAG_RSDB_BATCH* rsdb_batch, const char* key, size_t klen, const char* value, size_t vlen)
{
    rocksdb_writebatch_put(rsdb_batch->writebatch, key, klen, value, vlen);
    return XDAG_RSDB_OP_SUCCESS;
}


extern int xdag_rsdb_write(XDAG_RSDB* db, XDAG_RSDB_BATCH* batch)
{
    char *errmsg = NULL;
    
    rocksdb_write(db->db, db->write_options, batch->writebatch, &errmsg);
    if(errmsg)
    {
        return XDAG_RSDB_WRITE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_pre_init(void)
{
    XDAG_RSDB* rsdb = NULL;
    XDAG_RSDB_CONF* rsdb_config = NULL;

    rsdb = malloc(sizeof(XDAG_RSDB));
    rsdb_config = malloc(sizeof(XDAG_RSDB_CONF));
    memset(rsdb, 0, sizeof(XDAG_RSDB));
    memset(rsdb_config, 0, sizeof(XDAG_RSDB_CONF));

    rsdb->config = rsdb_config;

    rsdb->config->db_name = strdup("rsdb");
    rsdb->config->db_path = strdup("rsdb");
    rsdb->config->db_backup_path = strdup("rsdb_backup");

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

int xdag_rsdb_init(void)
{
    char key[1] ={[0]=SETTING_CREATED};
    char value[32] = {0};
    size_t vlen = 0;
    
    xdag_rsdb_getkey(g_xdag_rsdb, key, sizeof(key), value, &vlen);
    
    if(strncmp("done", value, strlen("done"))) {
        char stats_k[1] = {[0] = SETTING_STATS};
        xdag_rsdb_putkey(g_xdag_rsdb, stats_k, sizeof(key), (char*)&g_xdag_stats, sizeof(g_xdag_stats));
        xdag_rsdb_putkey(g_xdag_rsdb, key, sizeof(key), "done", strlen("done"));
        return XDAG_RSDB_INIT_NEW;
    } else {
        xdag_rsdb_load(g_xdag_rsdb);
        return XDAG_RSDB_INIT_LOAD;
    }
}

int xdag_rsdb_put_stats(XDAG_RSDB* rsdb)
{
    char stats_k[1] = {[0] = SETTING_STATS};
    xdag_rsdb_putkey(rsdb, stats_k, sizeof(stats_k), (char*)&g_xdag_stats, sizeof(g_xdag_stats));
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_get_bi(XDAG_RSDB* rsdb, xdag_hashlow_t hash, struct block_internal* bi)
{
    int retcode = 0;
    size_t vlen = 0;
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_BLOCK_INTERNAL};
    memcpy(key + 1, hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_getkey(rsdb, key, sizeof(key), (char*)bi, &vlen);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_get_orpbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash, struct block_internal* bi, struct xdag_block* xb)
{
    int retcode = 0;
   
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_ORP_BLOCK_INTERNAL};
    char val[sizeof(struct block_internal) + sizeof(struct xdag_block)] = {0};
    size_t vlen = 0;
    memcpy(key + 1, hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_getkey(rsdb, key, sizeof(key), (char*)val, &vlen);
    if(retcode) {
        return retcode;
    }
    memcpy(bi, val, sizeof(struct block_internal));
    memcpy(xb, val + sizeof(struct block_internal), sizeof(struct xdag_block));
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_del_orpbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash)
{
    int retcode = 0;
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_ORP_BLOCK_INTERNAL};
    memcpy(key + 1, hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_delkey(rsdb, key, 1+sizeof(xdag_hashlow_t));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_get_ourbi(XDAG_RSDB* rsdb, xdag_hashlow_t hash, struct block_internal* bi)
{
    int retcode = 0;
    
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_OUR_BLOCK_INTERNAL};
    char val[ sizeof(struct block_internal)] = {0};
    size_t vlen = 0;
    memcpy(key + 1, hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_getkey(rsdb, key, sizeof(key), val, &vlen);
    if(retcode) {
        return retcode;
    }
    memcpy(bi, val, sizeof(struct block_internal));
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_put_bi(XDAG_RSDB* rsdb, struct block_internal* bi)
{
    int retcode = 0;
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_putkey(rsdb, key, sizeof(key), (char*)bi, sizeof(struct block_internal));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_put_orpbi(XDAG_RSDB* rsdb, struct block_internal* bi, struct xdag_block* xb)
{
    int retcode = 0;
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_ORP_BLOCK_INTERNAL};
    const size_t vlen = sizeof(struct block_internal) + sizeof(struct xdag_block);
    char val[vlen] = {0};
    
    memcpy(key + 1, bi->hash, sizeof(xdag_hashlow_t));
    memcpy(val, bi, sizeof(struct block_internal));
    memcpy(val + sizeof(struct block_internal), xb, sizeof(struct xdag_block));
    
    retcode = xdag_rsdb_putkey(rsdb, key, sizeof(key), val, vlen);
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_put_ourbi(XDAG_RSDB* rsdb, struct block_internal* bi)
{
    int retcode = 0;
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_OUR_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, sizeof(xdag_hashlow_t));
    retcode = xdag_rsdb_putkey(rsdb, key, sizeof(key), (char*)bi, sizeof(struct block_internal));
    if(retcode) {
        return retcode;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_writebatch_put_bi(XDAG_RSDB_BATCH* batch, struct block_internal* bi)
{
    char key[ 1 + sizeof(xdag_hashlow_t) ] = {[0] = HASH_BLOCK_INTERNAL};
    memcpy(key + 1, bi->hash, sizeof(xdag_hashlow_t));
    rocksdb_writebatch_put(batch->writebatch, key, sizeof(key), (const char*)bi, sizeof(struct block_internal));
    return XDAG_RSDB_OP_SUCCESS;
}

//int xdag_rsdb_writebatch_merge_bi(XDAG_RSDB_BATCH* batch, struct block_internal* bi)
//{
//    char key[ 1 + sizeof(uint64_t) ] = {[0] = HASH_BLOCK_INTERNAL};
//    memcpy(key + 1, bi->hash, sizeof(uint64_t));
//    rocksdb_writebatch_merge(batch->writebatch, key, 33, (const char*)bi, sizeof(struct block_internal));
//    return XDAG_RSDB_OP_SUCCESS;
//}
