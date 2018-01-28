/* адреса, T13.692-T13.692 $DVS:time$ */

#ifndef CHEATCOIN_ADDRESS_H
#define CHEATCOIN_ADDRESS_H

#include "hash.h"

/* инициализировать модуль адресов */
extern int cheatcoin_address_init(void);

/* преобразовать адрес в хеш */
extern int cheatcoin_address2hash(const char *address, cheatcoin_hash_t hash);

/* преобразовать хеш в адрес */
extern const char *cheatcoin_hash2address(const cheatcoin_hash_t hash);

#endif
