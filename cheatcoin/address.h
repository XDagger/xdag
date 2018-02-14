/* address, T13.692-T13.692 $DVS:time$  */

#ifndef CHEATCOIN_ADDRESS_H
#define CHEATCOIN_ADDRESS_H

#include "hash.h"

/* initialize the address module */
extern int cheatcoin_address_init(void);

/* convert address to hash */
extern int cheatcoin_address2hash(const char *address, cheatcoin_hash_t hash);

/* convert hash to address */
extern const char *cheatcoin_hash2address(const cheatcoin_hash_t hash);

#endif
