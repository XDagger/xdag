//
//  load_block_test.h
//  xdag
//
//  Created by reymondtu on 2019/9/27.
//  Copyright © 2019年 xrdavies. All rights reserved.
//
#include "init.h"
#include "block.h"
#include "math.h"
#include "storage.h"
#include "time.h"
#include "global.h"
#include "wallet.h"
#include "address.h"
#include "../client/utils/random.h"
#include "crypt.h"
#include "../client/utils/log.h"
#include "memory.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#ifndef load_block_test_h
#define load_block_test_h

int unit_test_init();

void unit_test_create_blocks(xdag_time_t stime, int step, int count);

uint64_t unit_test_load_blocks(xdag_time_t stime, xdag_time_t etime, void *(*callback)(void *, void *));

struct xdag_out_balances_data {
    struct xdag_field *blocks;
    unsigned blocksCount, maxBlocksCount;
};

int xdag_out_balances_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time);

int xdag_out_sort_callback(const void *l, const void *r);

int xdag_out_balances(const char* file_name);

int xdag_load_case1();

int xdag_load_case2();

int test_case();

#endif /* load_block_test_h */
