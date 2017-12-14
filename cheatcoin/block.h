/* работа с блоками, T13.654-T13.720 $DVS:time$ */

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

/* начало регулярной обработки блоков; n_maining_threads - число потоков для майнинга на CPU */
extern int cheatcoin_blocks_start(int n_maining_threads);

/* проверить блок и включить его в базу данных, возвращает не 0 в случае ошибки */
extern int cheatcoin_add_block(struct cheatcoin_block *b);

/* для каждого своего блока вызывается callback */
extern int cheatcoin_traverse_our_blocks(void *data, int (*callback)(void *data, cheatcoin_hash_t hash,
		cheatcoin_amount_t amount, int n_our_key));

/* создать и опубликовать блок; в первых ninput полях fields содержатся адреса входов и соотв. кол-во читкоинов,
 * в следующих noutput полях - аналогично - выходы; fee - комиссия; send_time - время отправки блока; если оно больше текущего, то
 * проводится майнинг для генерации наиболее оптимального хеша */
extern int cheatcoin_create_block(struct cheatcoin_field *fields, int ninput, int noutput, cheatcoin_amount_t fee, cheatcoin_time_t send_time);

/* возвращает баланс адреса, или всех наших адресов, если hash == 0 */
extern cheatcoin_amount_t cheatcoin_get_balance(cheatcoin_hash_t hash);

/* по данному кол-ву главных блоков возвращает объем циркулирующих читкоинов */
extern cheatcoin_amount_t cheatcoin_get_supply(uint64_t nmain);

/* по хешу блока возвращает его позицию в хранилище и время */
extern int64_t cheatcoin_get_block_pos(const cheatcoin_hash_t hash, cheatcoin_time_t *time);

#endif
