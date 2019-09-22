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
#include <stdlib.h>
#include <unistd.h>

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

void unit_test_load_blocks(xdag_time_t stime, xdag_time_t etime)
{
    xdag_load_blocks(stime, etime, &stime, &add_block_callback1);
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
    xdag_time_t stime = XDAG_ERA;
    xdag_time_t etime = stime + count * 64 * 1000;

    unit_test_init();
    start = clock();
    unit_test_create_blocks(stime, 64 * 1000 , count);
    end = clock();
    printf("save %d blocks cost:%f sec\n", count, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之


    start = clock();
    unit_test_load_blocks(stime, etime);
    end = clock();
    printf("load %d blocks cost:%f sec\n", count, (end - start) / CLOCKS_PER_SEC);//以秒为单位显示之

	return EXIT_SUCCESS;
}
