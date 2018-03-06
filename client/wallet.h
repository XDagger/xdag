/* кошелёк, T13.681-T13.726 $DVS:time$ */

#ifndef XDAG_WALLET_H
#define XDAG_WALLET_H

#include "block.h"

struct xdag_public_key {
	void *key;
	uint64_t *pub; /* lower bit contains parity */
};

/* initializes a wallet */
extern int xdag_wallet_init(void);

/* generates a new key and sets is as defauld, returns its index */
extern int xdag_wallet_new_key(void);

/* returns a default key, the index of the default key is written to *n_key */
extern struct xdag_public_key *xdag_wallet_default_key(int *n_key);

/* returns an array of our keys */
extern struct xdag_public_key *xdag_wallet_our_keys(int *pnkeys);

/* completes work with wallet */
extern void xdag_wallet_finish(void);

#endif
