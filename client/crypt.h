/* криптография, T13.654-T13.826 $DVS:time$ */

#ifndef CHEATCOIN_CRYPT_H
#define CHEATCOIN_CRYPT_H

#include <stdint.h>
#include "hash.h"

/* инициализация системы шифрования */
extern int cheatcoin_crypt_init(int withrandom);

/* создать новую пару из закрытого и открытого ключа; возвращает указатель на его внутреннее представление, приватный ключ
 * сохраняет в массив privkey, публичный - в массив pubkey, чётность публичного ключа - в переменную pubkey_bit
*/
extern void *cheatcoin_create_key(cheatcoin_hash_t privkey, cheatcoin_hash_t pubkey, uint8_t *pubkey_bit);

/* возвращает внутреннее представление ключа и публичный ключ по известному приватному ключу */
extern void *cheatcoin_private_to_key(const cheatcoin_hash_t privkey, cheatcoin_hash_t pubkey, uint8_t *pubkey_bit);

/* возвращает внутреннее представление ключа по известному публичному ключу */
extern void *cheatcoin_public_to_key(const cheatcoin_hash_t pubkey, uint8_t pubkey_bit);

/* удаляет внутреннее представление ключа */
extern void cheatcoin_free_key(void *key);

/* подписать хеш и результат поместить в sign_r и sign_s */
extern int cheatcoin_sign(const void *key, const cheatcoin_hash_t hash, cheatcoin_hash_t sign_r, cheatcoin_hash_t sign_s);

/* проверить, что подпись (sign_r, sign_s) соответствует хешу hash, версия для собственного ключа; возвращает 0 при успехе */
extern int cheatcoin_verify_signature(const void *key, const cheatcoin_hash_t hash, const cheatcoin_hash_t sign_r, const cheatcoin_hash_t sign_s);

#endif
