#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // sysconf() - get CPU count
#include <pthread.h>

#include "rsdb.h"

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
    rsdb->restore_options = rocksdb_restore_options_create();
    rsdb->backup_engine = rocksdb_backup_engine_open(rsdb->options, rsdb->config->db_backup_path, &errmsg);
    if(errmsg)
    {
        printf("error:%s\n", errmsg);
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
        printf("error:%s\n", errmsg);
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
        printf("rocksdb_backup_engine_create_new_backup error:%s\n", errmsg);
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
        printf("error:%s\n", errmsg);
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


int xdag_rsdb_create(XDAG_RSDB* db)
{
    
    char key[32] = {0};
    char value[32] = {0};

    XDAG_RSDB_KEY_TYPE key_type = SETTING_CREATED;
    
    xdag_rsdb_getkey(key_type, NULL, key, 0);
    // put XDAG_ERA
    
    if(strncmp(value, "done", strlen("done")) == 0)
    {
        xdag_rsdb_put(db, key, "done");
    }
    
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_load(XDAG_RSDB* db)
{
    char data[32] = {0};

    XDAG_RSDB_KEY_TYPE key_type = SETTING_MAIN_HEAD;
    const char* key = xdag_rsdb_getkey(key_type, NULL, data, 0);
    xdag_rsdb_get(db, key, data, 32);
    
    
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_getkey(XDAG_RSDB_KEY_TYPE key_type, const char* key, char* new_key, size_t size)
{
    char * key_with_type = malloc(size + 1);
    key_with_type[0] = key_type;
    if(key && size > 0)
    {
        memcpy(key_with_type + 1, key, size);
    }
    new_key = key_with_type;
    return 0;
}


int xdag_rsdb_put(XDAG_RSDB* rsdb, const char* key, const char* value)
{
    char *errmsg = NULL;
    
    rocksdb_put(rsdb->db,
                rsdb->write_options,
                key, strlen(key),
                value, strlen(value) + 1,
                &errmsg);
    if(errmsg)
    {
        printf("rsdb put error:%s\n", errmsg);
        return XDAG_RSDB_PUT_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_get(XDAG_RSDB* rsdb, const char* key, char* data, size_t* len)
{
    char *errmsg = NULL;

    char *return_value = rocksdb_get(rsdb->db, rsdb->read_options, key, strlen(key), len, &errmsg);
    
    memcpy(data, return_value, *len);
    if(errmsg)
    {
        printf("rsdb get error:%s\n", errmsg);
        return XDAG_RSDB_PUT_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_delete(const XDAG_RSDB* db, const char* key)
{
    char *errmsg = NULL;
    
    rocksdb_delete(db->db, db->write_options, key, strlen(key), errmsg);
    if(errmsg)
    {
        printf("rsdb delete error:%s\n", errmsg);
        return XDAG_RSDB_DELETE_ERROR;
    }
    return XDAG_RSDB_OP_SUCCESS;
}

int xdag_rsdb_init(const XDAG_RSDB* db)
{
    XDAG_RSDB* rsdb = NULL;
    XDAG_RSDB_CONF* rsdb_config = NULL;
    char* create_key = NULL;
    char value[32] = {0};
    
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
        printf("xdag_rsdb_conf error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_init success\n");
    
    if((error_code = xdag_rsdb_open(rsdb))) {
        printf("xdag_rsdb_open error code is:%d\n", error_code);
        return error_code;
    }
    
    xdag_rsdb_getkey(SETTING_CREATED, NULL, create_key, 0);
    
    xdag_rsdb_get(rsdb, create_key, value, 32);
    
    if(!strncmp("done", value, strlen("done"))) {
        char* last_main_block_key = NULL;
        char* verify_main_block_key = NULL;
        
        xdag_rsdb_getkey(SETTING_MAIN_HEAD, NULL, last_main_block_key, 0);
        xdag_rsdb_getkey(SETTING_VERIFIED_MAIN_HEAD, NULL, verify_main_block_key, 0);
        
        xdag_rsdb_put(rsdb, last_main_block_key, "done");
        xdag_rsdb_put(rsdb, verify_main_block_key, "done");
        xdag_rsdb_put(rsdb, create_key, "done");
        
    } else {
        xdag_rsdb_load(rsdb);
    }
    
    return XDAG_RSDB_OP_SUCCESS;
}
