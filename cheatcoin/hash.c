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

void cheatcoin_hash_init(void *ctxv) {
	SHA256_CTX *ctx = (SHA256_CTX *)ctxv;
	sha256_init(ctx);
}

void cheatcoin_hash_update(void *data, size_t size, void *ctxv) {
	SHA256_CTX *ctx = (SHA256_CTX *)ctxv;
	sha256_update(ctx, data, size);
}

void cheatcoin_hash_final(void *data, size_t size, void *ctxv, cheatcoin_hash_t hash) {
	SHA256_CTX ctx;
	memcpy(&ctx, ctxv, sizeof(ctx));
	sha256_update(&ctx, (uint8_t *)data, size);
	sha256_final(&ctx, (uint8_t *)hash);
	sha256_init(&ctx);
	sha256_update(&ctx, (uint8_t *)hash, sizeof(cheatcoin_hash_t));
	sha256_final(&ctx, (uint8_t *)hash);
}

uint64_t cheatcoin_hash_final_multi(void *ctxv, uint64_t *nonce, int attempts, cheatcoin_hash_t hash) {
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

void cheatcoin_hash_get_state(void *ctxv, cheatcoin_hash_t state) {
	SHA256_CTX *ctx = (SHA256_CTX *)ctxv;
	memcpy(state, ctx->state, sizeof(cheatcoin_hash_t));
}

void cheatcoin_hash_set_state(void *ctxv, cheatcoin_hash_t state, size_t size) {
	SHA256_CTX *ctx = (SHA256_CTX *)ctxv;
	memcpy(ctx->state, state, sizeof(cheatcoin_hash_t));
	ctx->datalen = 0;
	ctx->bitlen = size << 3;
}
