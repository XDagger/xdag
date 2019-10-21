//
//  rsdb_test.c
//  xdag
//
//  Created by reymondtu on 2019/9/29.
//  Copyright © 2019 xrdavies. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "../client/rsdb.h"
#include "../client/global.h"

//struct block_internal {
//    xdag_hash_t hash;
//    xdag_diff_t difficulty;
//    xdag_amount_t amount, linkamount[15], fee;
//    xtime_t time;
//    uint64_t storage_pos;
//    union {
//        struct block_internal *ref;
//        struct orphan_block *oref;
//    };
//    struct block_internal *link[15];
//    atomic_uintptr_t backrefs;
//    atomic_uintptr_t remark;
//    uint16_t flags, in_mask, n_our_key;
//    uint8_t nlinks:4, max_diff_link:4, reserved;
//};

char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
void rand_str(char *dest, size_t length)
{
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

void test_writebatch(XDAG_RSDB* rsdb)
{
    char test_data[32] = {0};
    int error_code = 0;
    
    const char* test_batch_key1 = "test_batch_key1";
    const char* test_batch_value1 = "test_batch_value1";
    
    const char* test_batch_key2 = "test_batch_key2";
    const char* test_batch_value2 = "test_batch_value2";
    XDAG_RSDB_BATCH* batch = malloc(sizeof(XDAG_RSDB_BATCH));
    batch = xdag_rsdb_writebatch_new();
    xdag_rsdb_writebatch_put(batch, test_batch_key1, strlen(test_batch_key1), test_batch_value1, strlen(test_batch_value1));

    xdag_rsdb_writebatch_put(batch, test_batch_key2, strlen(test_batch_key2), test_batch_value2, strlen(test_batch_value2));

    xdag_rsdb_write(rsdb, batch);

    size_t tlen = 0;
    if((error_code = xdag_rsdb_getkey(rsdb, test_batch_key1, strlen(test_batch_key1), test_data, &tlen))) {
        printf("xdag_rsdb_get error code is:%d\n", error_code);
    }
    if(strncmp(test_batch_value1, test_data, strlen(test_data))) {
        return;
    }
    printf("value:%s, len:%d\n", test_data, (int)tlen);
    printf("test_writebatch success\n");
    free(batch);
}

void test_xdag_block(XDAG_RSDB* rsdb, int count)
{
    char key[32] = {0};
    const size_t bs = sizeof(struct block_internal);
    key[0] = 1;
    rand_str(key+1, sizeof(key) -1 );
    
    size_t write_size = 0;
    for(int i = 0; i < count; i++) {
        write_size+=bs;
        struct block_internal* b = malloc(bs);
        xdag_rsdb_putkey(rsdb, key, sizeof(key), (char*)b, sizeof(bs));
        
        struct block_internal r = {0};
        size_t vlen = 0;
        xdag_rsdb_getkey(rsdb, key, sizeof(key), (char*)&r , &vlen);
        
        if(!memcmp(b, &r, sizeof(struct block_internal))) {
            printf("test_xdag_block put/get not cmp!\n");
        }
        
    }
    printf("write %d block_internal ,size %zu to rocksdb.\n", count, write_size);
}

int xdag_rsdb_test()
{
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    char test_data[32] = {0};
    XDAG_RSDB* rsdb = NULL;
    
    char* db_name = "test_rsdb";
    char* db_path = "rsdb";
    char* db_backup_path = "rsdb_backup";
    
    rsdb = xdag_rsdb_new(db_name, db_path, db_backup_path);
    
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
    printf("xdag_rsdb_open success\n");
    
    if((error_code = xdag_rsdb_putkey(rsdb, test_key, strlen(test_key), test_value, strlen(test_value)))) {
        printf("xdag_rsdb_put error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_put success\n");
    
    size_t vlen = 0;
    if((error_code = xdag_rsdb_getkey(rsdb, test_key, strlen(test_key), test_data, &vlen))) {
        printf("xdag_rsdb_get error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_get success\n");
    
    if(strncmp(test_data, test_value, sizeof(test_data))) {
        printf("strcmp put(%s, %s) and get(%s) value is:%s\n", test_key, test_value, test_key, test_data);
        return -1;
    }

    if((error_code = xdag_rsdb_backup(rsdb))) {
        printf("xdag_rsdb_backup error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_backup success\n");
    
    if((error_code = xdag_rsdb_restore(rsdb))) {
        printf("xdag_rsdb_restore error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_restore success\n");
    
    if((error_code = xdag_rsdb_open(rsdb))) {
        printf("xdag_rsdb_open (reopen for restore) error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_open (reopen for restore) success\n");
    
    vlen = 0;
    memset(test_data, 0, sizeof(test_data));
    if((error_code = xdag_rsdb_getkey(rsdb, test_key, strlen(test_key), test_data, &vlen))) {
        printf("xdag_rsdb_get (reget for restore) error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_get (reget for restore) success\n");
    
    test_writebatch(rsdb);
    
    test_xdag_block(rsdb, 10000000);
    
    xdag_rsdb_close(rsdb);
    free(rsdb->config);
    free(rsdb);
    printf("xdag_rsdb test all case success\n");
    return 0;
}



int main(int argc, char** argv)
{
    xdag_rsdb_test();

    return EXIT_SUCCESS;
}
