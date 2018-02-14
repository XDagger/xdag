/* работа с блоками, T13.654-T13.836 $DVS:time$ */

#ifndef CHEATCOIN_BLOCK_H
#define CHEATCOIN_BLOCK_H

#include <stdint.h>
#include <stdio.h>
#include "hash.h"

enum cheatcoin_field_type
{
    CHEATCOIN_FIELD_NONCE,
    CHEATCOIN_FIELD_HEAD,
    CHEATCOIN_FIELD_IN,
    CHEATCOIN_FIELD_OUT,
    CHEATCOIN_FIELD_SIGN_IN,
    CHEATCOIN_FIELD_SIGN_OUT,
    CHEATCOIN_FIELD_PUBLIC_KEY_0,
    CHEATCOIN_FIELD_PUBLIC_KEY_1,
};

enum cheatcoin_message_type
{
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

struct cheatcoin_field
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    uint64_t transport_header;
                    uint64_t type;
                    cheatcoin_time_t time;
                };
                cheatcoin_hashlow_t hash;
            };
            union
            {
                cheatcoin_amount_t amount;
                cheatcoin_time_t end_time;
            };
        };
        cheatcoin_hash_t data;
    };
};

struct cheatcoin_block
{
    struct cheatcoin_field field[CHEATCOIN_BLOCK_FIELDS];
};

#define cheatcoin_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

/* начало регулярной обработки блоков; n_mining_threads - число потоков для майнинга на CPU;
 * для лёгкой ноды n_mining_threads < 0 и число потоков майнинга равно ~n_mining_threads;
 * miner_address = 1 - явно задан адрес майнера */
extern int cheatcoin_blocks_start(int n_mining_threads, int miner_address);

// checks and adds block to the storage. Return non-zero value in case of error.
extern int cheatcoin_add_block(struct cheatcoin_block *b);

//returns our first block. If there is no blocks yet - the first block is created.
extern int cheatcoin_get_our_block(cheatcoin_hash_t hash);

//calls callback for each own block
extern int cheatcoin_traverse_our_blocks(void *data,
    int(*callback)(void *, cheatcoin_hash_t, cheatcoin_amount_t, cheatcoin_time_t, int));

//calls callback for each block
extern int cheatcoin_traverse_all_blocks(void *data, int(*callback)(void *data, cheatcoin_hash_t hash,
    cheatcoin_amount_t amount, cheatcoin_time_t time));

/* создать и опубликовать блок; в первых ninput полях fields содержатся адреса входов и соотв. кол-во читкоинов,
 * в следующих noutput полях - аналогично - выходы; fee - комиссия; send_time - время отправки блока; если оно больше текущего, то
 * проводится майнинг для генерации наиболее оптимального хеша */
extern int cheatcoin_create_block(struct cheatcoin_field *fields, int ninput, int noutput, cheatcoin_amount_t fee, cheatcoin_time_t send_time);

//returns current balance for specified address or balance for all addresses if hash == 0
extern cheatcoin_amount_t cheatcoin_get_balance(cheatcoin_hash_t hash);

//sets current balance for the specified address
extern int cheatcoin_set_balance(cheatcoin_hash_t hash, cheatcoin_amount_t balance);

//returns current supply by specified count of main blocks
extern cheatcoin_amount_t cheatcoin_get_supply(uint64_t nmain);

//returns position and time of block by hash
extern int64_t cheatcoin_get_block_pos(const cheatcoin_hash_t hash, cheatcoin_time_t *time);

//returns a number of the current period, period is 64 seconds
extern cheatcoin_time_t cheatcoin_main_time(void);

/* возвращает номер периода времени, соответствующего старту сети */
extern cheatcoin_time_t cheatcoin_start_main_time(void);

//returns a number of key by hash of block or -1 if block is not our
extern int cheatcoin_get_key(cheatcoin_hash_t hash);

//reinitialization of block processing
extern int cheatcoin_blocks_reset(void);

//prints detailed information about block
extern int cheatcoin_print_block_info(cheatcoin_hash_t hash, FILE *out);

#endif
