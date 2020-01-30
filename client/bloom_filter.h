#ifndef XDAG_BLOOM_FILTER_H
#define XDAG_BLOOM_FILTER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct xdag_bloom_filter {
    char* cache;
    size_t size;
    uint32_t tc; // return true counter
    uint32_t fc; // return false counter
    uint32_t ac; // was added in cache counter
} XDAG_BLOOM_FILTER;

XDAG_BLOOM_FILTER* xdag_bloom_filter_new(void);

void xdag_bloom_filter_destroy(XDAG_BLOOM_FILTER* xbf);

int xdag_bloom_filter_setbit(XDAG_BLOOM_FILTER* xbf, char* k);

bool xdag_bloom_filter_getbit(XDAG_BLOOM_FILTER* xbf, char* k);

int xdag_bloom_filter_add(XDAG_BLOOM_FILTER* xbf, char* k);

bool xdag_bloom_filter_wasadded(XDAG_BLOOM_FILTER* xbf, char* k);
    
bool xdag_bloom_filter_init(void);

//XDAG_BLOOM_FILTER* g_xdag_bloom_filter_utxo;

#endif
