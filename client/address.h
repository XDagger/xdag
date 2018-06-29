/* addresses, T13.692-T13.692 $DVS:time$ */

#ifndef XDAG_ADDRESS_H
#define XDAG_ADDRESS_H

#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* intializes the addresses module */
extern int xdag_address_init(void);

/* converts address to hash */
extern int xdag_address2hash(const char *address, xdag_hash_t hash);

/* converts hash to address */
extern void xdag_hash2address(const xdag_hash_t hash, char *address);
	
#ifdef __cplusplus
};
#endif

#endif
