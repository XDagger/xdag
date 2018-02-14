/* purse, T13.681-T13.726 $ DVS: time $ */

#ifndef CHEATCOIN_WALLET_H
#define CHEATCOIN_WALLET_H

#include "block.h"

struct cheatcoin_public_key {
	void *key;
	uint64_t *pub; /* the least significant bit contains parity */
};

/* initialize a purse */
extern int cheatcoin_wallet_init(void);

/* generate a new key and set it to default, returns its number */
extern int cheatcoin_wallet_new_key(void);

/* returns the default key, in * n_key writes its number */
extern struct cheatcoin_public_key *cheatcoin_wallet_default_key(int *n_key);

/* returns an array of our keys */
extern struct cheatcoin_public_key *cheatcoin_wallet_our_keys(int *pnkeys);

/* finishes work with a purse */
extern void cheatcoin_wallet_finish(void);

#endif
