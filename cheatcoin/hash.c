/* хеш-функция, T13.654-T13.761 $DVS:time$ */

#include <string.h>
#include "sha256.h"
#include "hash.h"

void cheatcoin_hash(void *data, size_t size, cheatcoin_hash_t hash) {
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, data, size);
	sha256_final(&ctx, (uint8_t *)hash);
	sha256_init(&ctx);
	sha256_update(&ctx, (uint8_t *)hash, sizeof(cheatcoin_hash_t));
	sha256_final(&ctx, (uint8_t *)hash);
}

unsigned cheatcoin_hash_ctx_size(void) {
	return sizeof(SHA256_CTX);
}

void cheatcoin_prehash(void *data, size_t size, void *ctxv) {
	SHA256_CTX *ctx = (SHA256_CTX *)ctxv;
	sha256_init(ctx);
	sha256_update(ctx, data, size);
}

uint64_t cheatcoin_finalhash(void *ctxv, uint64_t *nonce, int attempts, cheatcoin_hash_t hash) {
	SHA256_CTX ctx;
	cheatcoin_hash_t hash0;
	uint64_t min_nonce = 0;
	int i;
	for (i = 0; i < attempts; ++i) {
		memcpy(&ctx, ctxv, sizeof(ctx));
		sha256_update(&ctx, (uint8_t *)nonce, sizeof(uint64_t));
		sha256_final(&ctx, (uint8_t *)hash0);
		sha256_init(&ctx);
		sha256_update(&ctx, (uint8_t *)hash0, sizeof(cheatcoin_hash_t));
		sha256_final(&ctx, (uint8_t *)hash0);
		if (!i || cheatcoin_cmphash(hash0, hash) < 0) {
			memcpy(hash, hash0, sizeof(cheatcoin_hash_t));
			min_nonce = *nonce;
		}
		++*nonce;
	}
	return min_nonce;
}
