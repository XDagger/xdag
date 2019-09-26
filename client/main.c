/* xdag main, T13.654-T13.895 $DVS:time$ */

#include "init.h"
#include "block.h"
#include "math.h"
#include "storage.h"
#include "time.h"
#include "wallet.h"
#include "address.h"
#include "utils/random.h"
#include "crypt.h"
#include "utils/log.h"
#include "memory.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int unit_test_init()
{
    if(xdag_address_init()) return -1;
    RandAddSeed();
    if(xdag_crypt_init()) return -1;
    if(xdag_wallet_init()) return -1;
    
    return 0;
}

void unit_test_create_blocks(xdag_time_t stime, int step, int count)
{
    xdag_time_t start_time = stime;
    for(int i = 0; i < count; i++) {
        struct xdag_block * b = xdag_create_block(0, 0, 0, 0, 0, start_time, NULL);
        xdag_storage_save(b);
        free(b);
        start_time+=step;
    }
}

uint64_t unit_test_load_blocks(xdag_time_t stime, xdag_time_t etime)
{
    return xdag_load_blocks(stime, etime, &stime, &add_block_callback1);
}

int xdag_traverse_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time)
{

    return 0;
}

static void *add_block_callback2(void *block, void *data)
{
    unsigned *i = (unsigned *)data;
    xdag_add_block((struct xdag_block *)block);
    if(!(++*i % 10000)) printf("blocks: %u\n", *i);
    return 0;
}

struct out_balances_data {
    struct xdag_field *blocks;
    unsigned blocksCount, maxBlocksCount;
};

static int out_balances_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time)
{
    struct out_balances_data *d = (struct out_balances_data *)data;
    struct xdag_field f;
    memcpy(f.hash, hash, sizeof(xdag_hashlow_t));
    f.amount = amount;
    if(!f.amount) {
        return 0;
    }
    if(d->blocksCount == d->maxBlocksCount) {
        d->maxBlocksCount = (d->maxBlocksCount ? d->maxBlocksCount * 2 : 0x100000);
        d->blocks = realloc(d->blocks, d->maxBlocksCount * sizeof(struct xdag_field));
    }
    memcpy(d->blocks + d->blocksCount, &f, sizeof(struct xdag_field));
    d->blocksCount++;
    return 0;
}

static int out_sort_callback(const void *l, const void *r)
{
    char address_l[33] = {0}, address_r[33] = {0};
    xdag_hash2address(((struct xdag_field *)l)->data, address_l);
    xdag_hash2address(((struct xdag_field *)r)->data, address_r);
    return strcmp(address_l, address_r);
}

int out_balances1()
{
    char address[33] = {0};
    struct out_balances_data d;
    unsigned i = 0;

    xdag_set_log_level(0);
    xdag_mem_init((xdag_get_frame() - xdag_get_start_frame()) << 17);
    xdag_crypt_init();
    memset(&d, 0, sizeof(struct out_balances_data));
    xdag_load_blocks(xdag_get_start_frame() << 16, xdag_get_frame() << 16, &i, &add_block_callback2);
    xdag_traverse_all_blocks(&d, out_balances_callback);

    qsort(d.blocks, d.blocksCount, sizeof(struct xdag_field), out_sort_callback);
    for(i = 0; i < d.blocksCount; ++i) {
        xdag_hash2address(d.blocks[i].data, address);
        printf("%s  %20.9Lf\n", address, amount2xdags(d.blocks[i].amount));
    }
    return 0;
}



int main(int argc, char **argv)
{
    
    //xdag_init(argc, argv, 0);
    // 4 years = 2097152 nmain
    //    uint64_t nmain = 189734;
    //
    //    printf("%Lf\n", amount2xdags(xdag_get_supply(nmain)));
    //    printf("%d\n", 1 << 21);
    
    // xdag main blocks 839418
    // xdag total blocks 315933451
    const int count = 100;
    double start, end;
    xdag_time_t stime = XDAG_TEST_ERA;
    //xdag_time_t etime = stime + count * 64 * 1000;
    xdag_time_t etime = xdag_get_xtimestamp();

    unit_test_init();
//    start = clock();
//    unit_test_create_blocks(stime, 64 * 1000 , count);
//    end = clock();
//    printf("save %d blocks cost:%f sec\n", count, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之


    start = clock();
    //uint64_t num = unit_test_load_blocks(stime, etime);
    uint64_t num = out_balances1();
    end = clock();
    printf("load %d blocks cost:%f sec\n", num, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之

    //xdag_traverse_all_blocks(&d, out_balances_callback);
	return EXIT_SUCCESS;
}
