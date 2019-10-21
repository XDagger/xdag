#include <string.h>
#include "bloom_filter.h"

XDAG_BLOOM_FILTER* xdag_bloom_filter_new()
{
    XDAG_BLOOM_FILTER* xbf = calloc(sizeof(XDAG_BLOOM_FILTER), 1);
    xbf->size = 1 << 27;
    xbf->cache = calloc(xbf->size, 1);
    return xbf;
}

void xdag_bloom_filter_destroy(XDAG_BLOOM_FILTER* xbf)
{
    free(xbf->cache);
    free(xbf);
}

int xdag_bloom_filter_setbit(XDAG_BLOOM_FILTER* xbf, char* key)
{
    int index = (key[0] & 0x3F) << 21 |
                (key[1] & 0xFF) << 13 |
                (key[2] & 0xFF) << 5  |
                (key[3] & 0xFF) >> 3;
    int bit = (key[3] & 0x07);
    int or_bit = (0x1 << bit);
    char new_key = (char)((int) xbf->cache[index] | or_bit);
    xbf->cache[index] = new_key;
    return 0;
}

bool xdag_bloom_filter_getbit(XDAG_BLOOM_FILTER* xbf, char* key)
{
    int index = (key[0] & 0x3F) << 21 |
                (key[1] & 0xFF) << 13 |
                (key[2] & 0xFF) << 5  |
                (key[3] & 0xFF) >> 3;
    int bit = (key[3] & 0x07);
    int or_bit = (0x1 << bit);
    char bkey = xbf->cache[index];
    return bkey & or_bit;
}

int xdag_bloom_filter_add(XDAG_BLOOM_FILTER* xbf, char* k)
{
    char first_hash[4] = {0};
    xbf->ac++;
    for (int i = 0; i < 3; i++) {
        //System.arraycopy(hash, i * 4, first_hash, 0, 4);
        memcpy(first_hash, k + i * 4, 4);
        xdag_bloom_filter_setbit(xbf, first_hash);
    }
    return 0;
}

bool xdag_bloom_filter_wasadded(XDAG_BLOOM_FILTER* xbf, char* k)
{
    char first_hash[4];
    for (int i = 0; i < 3; i++) {
        //System.arraycopy(hash.getBytes(), i * 4, firstHash, 0, 4);
        memcpy(first_hash, k + i * 4, 4);
        bool result = xdag_bloom_filter_getbit(xbf, first_hash);
        if (!result) {
            xbf->fc++;
            return false;
        }
    }
    xbf->tc++;
    return true;
}

bool xdag_bloom_filter_init()
{
//    g_xdag_bloom_filter = xdag_bloom_filter_new();
//    if(!g_xdag_bloom_filter) {
//        return false;
//    }
    return true;
}
