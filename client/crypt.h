/* cryptography, T13.654-T13.826 $DVS:time$ */

#ifndef CHEATCOIN_CRYPT_H
#define CHEATCOIN_CRYPT_H

#include <stdint.h>
#include "hash.h"

/* initialization of the encryption system */
extern int cheatcoin_crypt_init(int withrandom);

/* creates a new pair of private and public keys; returns a pointer to its internal representation,
 * the private key is saved to the 'privkey' array, the public key to the 'pubkey' array,
 * the parity of the public key is saved to the variable 'pubkey_bit'
 */
extern void *cheatcoin_create_key(cheatcoin_hash_t privkey, cheatcoin_hash_t pubkey, uint8_t *pubkey_bit);

/* returns the internal representation of the key and the public key by the known private key */
extern void *cheatcoin_private_to_key(const cheatcoin_hash_t privkey, cheatcoin_hash_t pubkey, uint8_t *pubkey_bit);

/* Returns the internal representation of the key by the known public key */
extern void *cheatcoin_public_to_key(const cheatcoin_hash_t pubkey, uint8_t pubkey_bit);

/* removes the internal key representation */
extern void cheatcoin_free_key(void *key);

/* sign the hash and put the result in sign_r and sign_s */
extern int cheatcoin_sign(const void *key, const cheatcoin_hash_t hash, cheatcoin_hash_t sign_r, cheatcoin_hash_t sign_s);

/*verify that the signature (sign_r, sign_s) corresponds to a hash 'hash', a version for its own key; returns 0 on success */
extern int cheatcoin_verify_signature(const void *key, const cheatcoin_hash_t hash, const cheatcoin_hash_t sign_r, const cheatcoin_hash_t sign_s);

#endif
