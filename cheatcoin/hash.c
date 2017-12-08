/* хеш-функция, T13.654-T13.671 $DVS:time$ */

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
