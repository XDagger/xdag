/* кошелёк, T13.681-T13.788 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "crypt.h"
#include "wallet.h"
#include "log.h"
#include "main.h"
#include "transport.h"

#define WALLET_FILE (g_cheatcoin_testnet ? "wallet-testnet.dat" : "wallet.dat")

struct key_internal {
	cheatcoin_hash_t pub, priv;
	void *key;
	struct key_internal *prev;
	uint8_t pub_bit;
};

static struct key_internal *def_key = 0;
static struct cheatcoin_public_key *keys_arr = 0;
static pthread_mutex_t wallet_mutex = PTHREAD_MUTEX_INITIALIZER;
int nkeys = 0, maxnkeys = 0;

static int add_key(cheatcoin_hash_t priv) {
	struct key_internal *k = malloc(sizeof(struct key_internal));
	if (!k) return -1;
	pthread_mutex_lock(&wallet_mutex);
	if (priv) {
		memcpy(k->priv, priv, sizeof(cheatcoin_hash_t));
		k->key = cheatcoin_private_to_key(k->priv, k->pub, &k->pub_bit);
	} else {
		FILE *f;
		uint32_t priv32[sizeof(cheatcoin_hash_t)/sizeof(uint32_t)];
		k->key = cheatcoin_create_key(k->priv, k->pub, &k->pub_bit);
		f = fopen(WALLET_FILE, "ab");
		if (!f) goto fail;
		memcpy(priv32, k->priv, sizeof(cheatcoin_hash_t));
		cheatcoin_user_crypt_action(priv32, nkeys, sizeof(cheatcoin_hash_t)/sizeof(uint32_t), 1);
		if (fwrite(priv32, sizeof(cheatcoin_hash_t), 1, f) != 1) { fclose(f); goto fail; }
		fclose(f);
	}
	if (!k->key) goto fail;
	k->prev = def_key;
	def_key = k;
	if (nkeys == maxnkeys) {
		struct cheatcoin_public_key *newarr = (struct cheatcoin_public_key *)
				realloc(keys_arr, ((maxnkeys | 0xff) + 1) * sizeof(struct cheatcoin_public_key));
		if (!newarr) goto fail;
		maxnkeys |= 0xff, maxnkeys++;
		keys_arr = newarr;
	}
	keys_arr[nkeys].key = k->key;
	keys_arr[nkeys].pub = (uint64_t *)((uintptr_t)&k->pub | k->pub_bit);
	cheatcoin_debug("Key %2d: priv=[%s] pub=[%02x:%s]", nkeys,
			cheatcoin_log_hash(k->priv), 0x02 + k->pub_bit,  cheatcoin_log_hash(k->pub));
	nkeys++;
	pthread_mutex_unlock(&wallet_mutex);
	return 0;
fail:
	pthread_mutex_unlock(&wallet_mutex);
	free(k);
	return -1;
}

/* сгенерировать новый ключ и установить его по умолчанию, возвращает его номер */
int cheatcoin_wallet_new_key(void) {
	int res = add_key(0);
	if (!res) res = nkeys - 1;
	return res;
}

/* инициализировать кошелёк */
int cheatcoin_wallet_init(void) {
	uint32_t priv32[sizeof(cheatcoin_hash_t)/sizeof(uint32_t)];
	cheatcoin_hash_t priv;
	FILE *f = fopen(WALLET_FILE, "rb");
	int n;
	if (!f) {
		if (add_key(0)) return -1;
		f = fopen(WALLET_FILE, "r");
		if (!f) return -1;
		fread(priv32, sizeof(cheatcoin_hash_t), 1, f);
		n = 1;
	} else n = 0;
	while (fread(priv32, sizeof(cheatcoin_hash_t), 1, f) == 1) {
		cheatcoin_user_crypt_action(priv32, n++, sizeof(cheatcoin_hash_t)/sizeof(uint32_t), 2);
		memcpy(priv, priv32, sizeof(cheatcoin_hash_t));
		add_key(priv);
	}
	fclose(f);
	return 0;
}

/* возвращает ключ по умолчанию, в *n_key записывает его номер */
struct cheatcoin_public_key *cheatcoin_wallet_default_key(int *n_key) {
	if (nkeys) { if (n_key) *n_key = nkeys - 1; return keys_arr + nkeys - 1; }
	return 0;
}

/* возвращает массив наших ключей */
struct cheatcoin_public_key *cheatcoin_wallet_our_keys(int *pnkeys) {
	*pnkeys = nkeys;
	return keys_arr;
}

/* завершает работу с кошельком */
void cheatcoin_wallet_finish(void) {
	pthread_mutex_lock(&wallet_mutex);
}
