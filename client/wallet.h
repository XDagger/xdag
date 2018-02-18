/* кошелёк, T13.681-T13.726 $DVS:time$ */

#ifndef CHEATCOIN_WALLET_H
#define CHEATCOIN_WALLET_H

#include "block.h"

struct cheatcoin_public_key {
	void *key;
	uint64_t *pub; /* lower bit contains parity */
};

/* initializes a wallet */
extern int cheatcoin_wallet_init(void);

/* generates a new key and sets is as defauld, returns its index */
extern int cheatcoin_wallet_new_key(void);

/* returns a default key, the index of the default key is written to *n_key */
extern struct cheatcoin_public_key *cheatcoin_wallet_default_key(int *n_key);

/* returns an array of our keys */
extern struct cheatcoin_public_key *cheatcoin_wallet_our_keys(int *pnkeys);

/* completes work with wallet */
extern void cheatcoin_wallet_finish(void);

#endif
