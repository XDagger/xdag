/* хеш-функция, T13.654-T13.761 $DVS:time$ */

#ifndef CHEATCOIN_HASH_H
#define CHEATCOIN_HASH_H

#include <stddef.h>
#include <stdint.h>
#include "system.h"

typedef uint64_t cheatcoin_hash_t[4];
typedef uint64_t cheatcoin_hashlow_t[3];

extern void cheatcoin_hash(void *data, size_t size, cheatcoin_hash_t hash);

static inline int cheatcoin_cmphash(cheatcoin_hash_t l, cheatcoin_hash_t r) {
	int i;
	for (i = 3; i >= 0; --i) if (l[i] != r[i]) return (l[i] < r[i] ? -1 : 1);
	return 0;
}

extern unsigned cheatcoin_hash_ctx_size(void);

extern void cheatcoin_prehash(void *data, size_t size, void *ctxv);

extern uint64_t cheatcoin_finalhash(void *ctxv, uint64_t *nonce, int attempts, cheatcoin_hash_t hash);

#endif
