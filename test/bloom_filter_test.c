#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../client/bloom_filter.h"

char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
void rand_str(char *dest, size_t length)
{
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}


void print_bloom_filter_stats(XDAG_BLOOM_FILTER* bf)
{
    printf("--------------------------\n");
    printf("bloom filter stats:\n");
    printf("add          count: %d\n", bf->ac);
    printf("return true  count: %d\n", bf->tc);
    printf("return false count: %d\n", bf->fc);
}

int main(int argc, char** argv)
{
    const size_t size = 10000000;
    const size_t strlen = 32;

    char** keys1 = calloc(sizeof(char**) * size, 1);
    char** keys2 = calloc(sizeof(char**) * size, 1);

    for(int i = 0; i < size; i++ ){
        keys1[i] = calloc(strlen, 1);
        keys2[i] = calloc(strlen, 1);
        rand_str(keys1[i], strlen);
        rand_str(keys2[i], strlen);
    }
    
    // This size chosen as with 3 functions we should only get 4% errors
    // with 150m entries.
    XDAG_BLOOM_FILTER* bloom_filter = xdag_bloom_filter_new();

    printf("bloom_filter->size = %zu\n", bloom_filter->size);
    int i = 0;
    for(i = 0; i < size; i++) {
        
        xdag_bloom_filter_add(bloom_filter, keys1[i]);
    }

    for(i = 0; i < size; i++) {
        if(xdag_bloom_filter_wasadded(bloom_filter, keys1[i]))
        {
//            printf("key:%s was added!\n", keys[i]);
        } else {
//            printf("key:%s not added!\n", keys[i]);
        }
    }
    
    for(i = 0; i < size; i++) {
        if(xdag_bloom_filter_wasadded(bloom_filter, keys2[i]))
        {
            //            printf("key:%s was added!\n", keys[i]);
        } else {
            //            printf("key:%s not added!\n", keys[i]);
        }
    }

    print_bloom_filter_stats(bloom_filter);

    xdag_bloom_filter_destroy(bloom_filter);
    return EXIT_SUCCESS;
}
