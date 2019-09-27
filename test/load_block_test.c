#include "load_block_test.h"

int unit_test_init()
{
    if(!xdag_time_init()) return -1;
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

uint64_t unit_test_load_blocks(xdag_time_t stime, xdag_time_t etime, void *(*callback)(void *, void *))
{
    return xdag_load_blocks(stime, etime, &stime, callback);
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

int xdag_out_balances(const char* file_name)
{
    char address[33] = {0};
    char msg[128] = {0};
    struct out_balances_data d;
    unsigned i = 0;
    
    xdag_set_log_level(0);
    xdag_mem_init((xdag_get_frame() - xdag_get_start_frame()) << 17);
    xdag_crypt_init();
    memset(&d, 0, sizeof(struct out_balances_data));
    //xdag_load_blocks(xdag_get_start_frame() << 16, xdag_get_frame() << 16, &i, &add_block_callback2);
    xdag_traverse_all_blocks(&d, out_balances_callback);
    
    qsort(d.blocks, d.blocksCount, sizeof(struct xdag_field), out_sort_callback);
    FILE* fp = fopen(file_name, "a");
    for(i = 0; i < d.blocksCount; ++i) {
        xdag_hash2address(d.blocks[i].data, address);
        snprintf(msg, sizeof(msg), "%s  %20.9Lf\n", address, amount2xdags(d.blocks[i].amount));
        fprintf(fp, "%s", msg);
        
    }
    fflush(fp);
    fclose(fp);
    return 0;
}

int xdag_load_case1()
{
    g_xdag_testnet = 1;
    double start, end;
    xdag_time_t stime = XDAG_TEST_ERA;
    xdag_time_t etime = xdag_get_xtimestamp();
    
    start = clock();
    uint64_t num = unit_test_load_blocks(stime, etime, &add_block_callback_sync);
    xdag_out_balances("balance1.txt");
    end = clock();
    printf("case1 load %d blocks cost:%f sec\n", num, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之
    
    return 0;
}

int xdag_load_case2()
{
    g_xdag_testnet = 1;
    double start, end;
    xdag_time_t stime = XDAG_TEST_ERA;
    xdag_time_t etime = xdag_get_xtimestamp();
    
    start = clock();
    uint64_t num = unit_test_load_blocks(stime, etime, &add_block_callback_nosync);
    xdag_out_balances("balance2.txt");
    end = clock();
    printf("case2 load %d blocks cost:%f sec\n", num, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之
    
    return 0;
}

int test_case()
{
    unit_test_init();
    g_xdag_state = XDAG_STATE_LOAD;
    //xdag_load_case1();
    xdag_load_case2();
    return 0;
}
