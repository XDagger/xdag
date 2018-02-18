/* processing of blocks, T13.654-T13.836 $DVS:time$ */

#ifndef XDAG_BLOCK_H
#define XDAG_BLOCK_H

#include <stdint.h>
#include <stdio.h>
#include "hash.h"

enum xdag_field_type
{
    XDAG_FIELD_NONCE,
    XDAG_FIELD_HEAD,
    XDAG_FIELD_IN,
    XDAG_FIELD_OUT,
    XDAG_FIELD_SIGN_IN,
    XDAG_FIELD_SIGN_OUT,
    XDAG_FIELD_PUBLIC_KEY_0,
    XDAG_FIELD_PUBLIC_KEY_1,
};

enum xdag_message_type
{
    XDAG_MESSAGE_BLOCKS_REQUEST,
    XDAG_MESSAGE_BLOCKS_REPLY,
    XDAG_MESSAGE_SUMS_REQUEST,
    XDAG_MESSAGE_SUMS_REPLY,
    XDAG_MESSAGE_BLOCKEXT_REQUEST,
    XDAG_MESSAGE_BLOCKEXT_REPLY,
    XDAG_MESSAGE_BLOCK_REQUEST,
};

#define XDAG_BLOCK_FIELDS 16

typedef uint64_t xdag_time_t;
typedef uint64_t xdag_amount_t;

struct xdag_field
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
                    xdag_time_t time;
                };
                xdag_hashlow_t hash;
            };
            union
            {
                xdag_amount_t amount;
                xdag_time_t end_time;
            };
        };
        xdag_hash_t data;
    };
};

struct xdag_block
{
    struct xdag_field field[XDAG_BLOCK_FIELDS];
};

#define xdag_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

/* start of regular block processing; n_mining_threads - the number of threads for mining on the CPU;
 * for the light node n_mining_threads < 0 and the number of threads is equal to ~n_mining_threads;
 * miner_address = 1 - the address of the miner is explicitly set 
 */
extern int xdag_blocks_start(int n_mining_threads, int miner_address);

/* checks and adds block to the storage. Returns non-zero value in case of error. */
extern int xdag_add_block(struct xdag_block *b);

/* returns our first block. If there is no blocks yet - the first block is created. */
extern int xdag_get_our_block(xdag_hash_t hash);

/* calls callback for each own block */
extern int xdag_traverse_our_blocks(void *data,
    int(*callback)(void *, xdag_hash_t, xdag_amount_t, xdag_time_t, int));

/* calls callback for each block */
extern int xdag_traverse_all_blocks(void *data, int(*callback)(void *data, xdag_hash_t hash,
    xdag_amount_t amount, xdag_time_t time));

/* create and publish a block; The first 'ninput' fields 'fields' contain the addresses of the inputs and the corresponding quiantity of XDag,
 * in the following 'noutput' fields similarly - outputs, fee; send_time (time of sending the block);
 * if it is greater than the current one, then the mining is performed to generate the most optimal hash
 */
extern int xdag_create_block(struct xdag_field *fields, int ninput, int noutput, xdag_amount_t fee, xdag_time_t send_time);

/* returns current balance for specified address or balance for all addresses if hash == 0 */
extern xdag_amount_t xdag_get_balance(xdag_hash_t hash);

/* sets current balance for the specified address */
extern int xdag_set_balance(xdag_hash_t hash, xdag_amount_t balance);

/* calculates current supply by specified count of main blocks */
extern xdag_amount_t xdag_get_supply(uint64_t nmain);

/*returns position and time of block by hash */
extern int64_t xdag_get_block_pos(const xdag_hash_t hash, xdag_time_t *time);

/* returns a number of the current period, period is 64 seconds */
extern xdag_time_t xdag_main_time(void);

/* returns the number of the time period corresponding to the start of the network */
extern xdag_time_t xdag_start_main_time(void);

/* returns a number of key by hash of block or -1 if block is not our */
extern int xdag_get_key(xdag_hash_t hash);

/* reinitialization of block processing */
extern int xdag_blocks_reset(void);

/* prints detailed information about block */
extern int xdag_print_block_info(xdag_hash_t hash, FILE *out);

#endif
