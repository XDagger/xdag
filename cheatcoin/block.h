/* work with blocks, T13.654-T13.836 $ DVS: time $ */

#ifndef CHEATCOIN_BLOCK_H
#define CHEATCOIN_BLOCK_H

#include <stdint.h>
#include <stdio.h>
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

/* start of regular block processing; n_mining_threads - the number of
 * threads to be mined on the CPU; for the easy node n_mining_threads < 0
 * and the number of threads is equal to ~ n_mining_threads;
 * miner_address = 1 - the address of the miner is explicitly set
 */
extern int cheatcoin_blocks_start(int n_mining_threads, int miner_address);

/* check the block and include it in the database, returns not 0 in case of an error */
extern int cheatcoin_add_block(struct cheatcoin_block *b);

/* gives out our first block, and if it does not, creates */
extern int cheatcoin_get_our_block(cheatcoin_hash_t hash);

/* Callback is called for each of its blocks */
extern int cheatcoin_traverse_our_blocks(
    void *data, int (*callback)(void *, cheatcoin_hash_t, cheatcoin_amount_t, cheatcoin_time_t, int));

/* callback is called for each block */
extern int cheatcoin_traverse_all_blocks(
					 void *data, int (*callback)(void *data, cheatcoin_hash_t hash,
		cheatcoin_amount_t amount, cheatcoin_time_t time));

/* create and publish a block; The first ninput fields fields contain the addresses of the inputs and the corresponding fields. number of citcoins, * in the following noutput fields - similarly - outputs; fee - commission; send_time - time of sending the block; if it is greater than the current one, then * the mining is performed to generate the most optimal hash */
extern int cheatcoin_create_block(struct cheatcoin_field *fields, int ninput, int noutput, cheatcoin_amount_t fee, cheatcoin_time_t send_time);

/* returns the balance of the address, or all of our addresses, if hash == 0 */
extern cheatcoin_amount_t cheatcoin_get_balance(cheatcoin_hash_t hash);

/* sets the address balance */
extern int cheatcoin_set_balance(cheatcoin_hash_t hash, cheatcoin_amount_t balance);

/* on this count of main blocks returns the volume of circulating citcoins */
extern cheatcoin_amount_t cheatcoin_get_supply(uint64_t nmain);

/* On the block hash it returns its position in the store and time */
extern int64_t cheatcoin_get_block_pos(const cheatcoin_hash_t hash, cheatcoin_time_t *time);

/* returns the number of the current time period, the period is 64 seconds */
extern cheatcoin_time_t cheatcoin_main_time(void);

/* returns the number of the time period corresponding to the start of the network */
extern cheatcoin_time_t cheatcoin_start_main_time(void);

/* on the hash of the block returns the key number or -1, if the block is not ours */
extern int cheatcoin_get_key(cheatcoin_hash_t hash);

/* re-initialize the block processing system */
extern int cheatcoin_blocks_reset(void);

/* display detailed information about the block */
extern int cheatcoin_print_block_info(cheatcoin_hash_t hash, FILE *out);

#endif
