/* hash function, T13.654-T13.775 $DVS:time$ */

#ifndef XDAG_HASH_H
#define XDAG_HASH_H

#include <stddef.h>
#include <stdint.h>
#include "system.h"

typedef uint64_t xdag_hash_t[4];
typedef uint64_t xdag_hashlow_t[3];

extern void xdag_hash(void *data, size_t size, xdag_hash_t hash);

static inline int xdag_cmphash(xdag_hash_t l, xdag_hash_t r)
{
    int i;
    for(i = 3; i >= 0; --i) if(l[i] != r[i]) return (l[i] < r[i] ? -1 : 1);
    return 0;
}

extern unsigned xdag_hash_ctx_size(void);

extern void xdag_hash_init(void *ctxv);

extern void xdag_hash_update(void *ctxv, void *data, size_t size);

extern void xdag_hash_final(void *ctxv, void *data, size_t size, xdag_hash_t hash);

extern uint64_t xdag_hash_final_multi(void *ctxv, uint64_t *nonce, int attempts, int step, xdag_hash_t hash);

extern void xdag_hash_get_state(void *ctxv, xdag_hash_t state);

extern void xdag_hash_set_state(void *ctxv, xdag_hash_t state, size_t size);

#endif
