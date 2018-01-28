/* кошелёк, T13.681-T13.726 $DVS:time$ */

#ifndef CHEATCOIN_WALLET_H
#define CHEATCOIN_WALLET_H

#include "block.h"

struct cheatcoin_public_key {
	void *key;
	uint64_t *pub; /* младший бит содержит чётность */
};

/* инициализировать кошелёк */
extern int cheatcoin_wallet_init(void);

/* сгенерировать новый ключ и установить его по умолчанию, возвращает его номер */
extern int cheatcoin_wallet_new_key(void);

/* возвращает ключ по умолчанию, в *n_key записывает его номер */
extern struct cheatcoin_public_key *cheatcoin_wallet_default_key(int *n_key);

/* возвращает массив наших ключей */
extern struct cheatcoin_public_key *cheatcoin_wallet_our_keys(int *pnkeys);

/* завершает работу с кошельком */
extern void cheatcoin_wallet_finish(void);

#endif
