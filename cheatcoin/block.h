/* работа с блоками, T13.654-T13.836 $DVS:time$ */

#ifndef CHEATCOIN_BLOCK_H
#define CHEATCOIN_BLOCK_H

#include <stdint.h>
#include "hash.h"

enum cheatcoin_field_type {
	CHEATCOIN_FIELD_NONCE,
	CHEATCOIN_FIELD_HEAD,
	CHEATCOIN_FIELD_IN,
	CHEATCOIN_FIELD_OUT,
	CHEATCOIN_FIELD_SIGN_IN,
	CHEATCOIN_FIELD_SIGN_OUT,
	CHEATCOIN_FIELD_PUBLIC_KEY_0,
	CHEATCOIN_FIELD_PUBLIC_KEY_1,
};

enum cheatcoin_message_type {
	CHEATCOIN_MESSAGE_BLOCKS_REQUEST,
	CHEATCOIN_MESSAGE_BLOCKS_REPLY,
	CHEATCOIN_MESSAGE_SUMS_REQUEST,
	CHEATCOIN_MESSAGE_SUMS_REPLY,
	CHEATCOIN_MESSAGE_BLOCKEXT_REQUEST,
	CHEATCOIN_MESSAGE_BLOCKEXT_REPLY,
	CHEATCOIN_MESSAGE_BLOCK_REQUEST,
};

#define CHEATCOIN_BLOCK_FIELDS 16

typedef uint64_t cheatcoin_time_t;
typedef uint64_t cheatcoin_amount_t;

struct cheatcoin_field {
	union {
		struct {
			union {
				struct {
					uint64_t transport_header;
					uint64_t type;
					cheatcoin_time_t time;
				};
				cheatcoin_hashlow_t hash;
			};
			union {
				cheatcoin_amount_t amount;
				cheatcoin_time_t end_time;
			};
		};
		cheatcoin_hash_t data;
	};
};

struct cheatcoin_block {
	struct cheatcoin_field field[CHEATCOIN_BLOCK_FIELDS];
};

#define cheatcoin_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

/* начало регулярной обработки блоков; n_mining_threads - число потоков для майнинга на CPU;
 * для лёгкой ноды n_mining_threads < 0 и число потоков майнинга равно ~n_mining_threads;
 * miner_address = 1 - явно задан адрес майнера */
extern int cheatcoin_blocks_start(int n_mining_threads, int miner_address);

/* проверить блок и включить его в базу данных, возвращает не 0 в случае ошибки */
extern int cheatcoin_add_block(struct cheatcoin_block *b);

/* выдаёт первый наш блок, а если его нет - создаёт */
extern int cheatcoin_get_our_block(cheatcoin_hash_t hash);

/* для каждого своего блока вызывается callback */
extern int cheatcoin_traverse_our_blocks(void *data,
		int (*callback)(void *, cheatcoin_hash_t, cheatcoin_amount_t, cheatcoin_time_t, int));

/* для каждого блока вызывается callback */
extern int cheatcoin_traverse_all_blocks(void *data, int (*callback)(void *data, cheatcoin_hash_t hash,
		cheatcoin_amount_t amount, cheatcoin_time_t time));

/* создать и опубликовать блок; в первых ninput полях fields содержатся адреса входов и соотв. кол-во читкоинов,
 * в следующих noutput полях - аналогично - выходы; fee - комиссия; send_time - время отправки блока; если оно больше текущего, то
 * проводится майнинг для генерации наиболее оптимального хеша */
extern int cheatcoin_create_block(struct cheatcoin_field *fields, int ninput, int noutput, cheatcoin_amount_t fee, cheatcoin_time_t send_time);

/* возвращает баланс адреса, или всех наших адресов, если hash == 0 */
extern cheatcoin_amount_t cheatcoin_get_balance(cheatcoin_hash_t hash);

/* устанавливает баланс адреса */
extern int cheatcoin_set_balance(cheatcoin_hash_t hash, cheatcoin_amount_t balance);

/* по данному кол-ву главных блоков возвращает объем циркулирующих читкоинов */
extern cheatcoin_amount_t cheatcoin_get_supply(uint64_t nmain);

/* по хешу блока возвращает его позицию в хранилище и время */
extern int64_t cheatcoin_get_block_pos(const cheatcoin_hash_t hash, cheatcoin_time_t *time);

/* возвращает номер текущего периода времени, пеиод - это 64 секунды */
extern cheatcoin_time_t cheatcoin_main_time(void);

/* возвращает номер периода времени, соответствующего старту сети */
extern cheatcoin_time_t cheatcoin_start_main_time(void);

/* по хешу блока возвращает номер ключа или -1, если блок не наш */
extern int cheatcoin_get_key(cheatcoin_hash_t hash);

/* переинициализация системы обработки блоков */
extern int cheatcoin_blocks_reset(void);

#endif
