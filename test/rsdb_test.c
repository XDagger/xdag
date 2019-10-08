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
#include "../client/rsdb.h"

int xdag_rsdb_test()
{
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    char test_data[128] = {0};
    XDAG_RSDB* rsdb = NULL;
    XDAG_RSDB_CONF* rsdb_config = NULL;
    
    rsdb = malloc(sizeof(XDAG_RSDB));
    rsdb_config = malloc(sizeof(XDAG_RSDB_CONF));
    memset(rsdb, 0, sizeof(XDAG_RSDB));
    memset(rsdb_config, 0, sizeof(XDAG_RSDB_CONF));
    
    rsdb->config = rsdb_config;
    
    rsdb->config->db_name = strdup("test_rsdb");
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
    printf("xdag_rsdb_open success\n");
    
    if((error_code = xdag_rsdb_put(rsdb, test_key, test_value))) {
        printf("xdag_rsdb_put error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_put success\n");
    
    size_t len = 0;
    if((error_code = xdag_rsdb_get(rsdb, test_key, test_data, &len))) {
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
    
    len = 0;
    if((error_code = xdag_rsdb_get(rsdb, test_key, test_data, &len))) {
        printf("xdag_rsdb_get (reget for restore) error code is:%d\n", error_code);
        return error_code;
    }
    printf("xdag_rsdb_get (reget for restore) success\n");
    
    xdag_rsdb_close(rsdb);
    free(rsdb->config);
    free(rsdb);
    printf("xdag_rsdb test all case success\n");
    return 0;
}

//int main(int argc, char** argv)
//{
//    xdag_rsdb_test();
//    return EXIT_SUCCESS;
//}
