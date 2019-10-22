/* block processing, T13.654-T14.618 $DVS:time$ */

#ifndef XDAG_BLOCK_H
#define XDAG_BLOCK_H

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "hash.h"
#include "system.h"
#include "types.h"
#include "utils/atomic.h"

enum xdag_field_type {
	XDAG_FIELD_NONCE,        //0
	XDAG_FIELD_HEAD,         //1
	XDAG_FIELD_IN,           //2
	XDAG_FIELD_OUT,          //3
	XDAG_FIELD_SIGN_IN,      //4
	XDAG_FIELD_SIGN_OUT,     //5
	XDAG_FIELD_PUBLIC_KEY_0, //6
	XDAG_FIELD_PUBLIC_KEY_1, //7
	XDAG_FIELD_HEAD_TEST,    //8
	XDAG_FIELD_REMARK,       //9
	XDAG_FIELD_RESERVE1,     //A
	XDAG_FIELD_RESERVE2,     //B
	XDAG_FIELD_RESERVE3,     //C
	XDAG_FIELD_RESERVE4,     //D
	XDAG_FIELD_RESERVE5,     //E
	XDAG_FIELD_RESERVE6      //F
};

enum xdag_message_type {
	XDAG_MESSAGE_BLOCKS_REQUEST,
	XDAG_MESSAGE_BLOCKS_REPLY,
	XDAG_MESSAGE_SUMS_REQUEST,
	XDAG_MESSAGE_SUMS_REPLY,
	XDAG_MESSAGE_BLOCKEXT_REQUEST,
	XDAG_MESSAGE_BLOCKEXT_REPLY,
	XDAG_MESSAGE_BLOCK_REQUEST,
};

enum bi_flags {
	BI_MAIN       = 0x01,
	BI_MAIN_CHAIN = 0x02,
	BI_APPLIED    = 0x04,
	BI_MAIN_REF   = 0x08,
	BI_REF        = 0x10,
	BI_OURS       = 0x20,
	BI_EXTRA      = 0x40,
	BI_REMARK     = 0x80
};

#define XDAG_BLOCK_FIELDS 16

#define REMARK_ENABLED 0

#if CHAR_BIT != 8
#error Your system hasn't exactly 8 bit for a char, it won't run.
#endif

typedef uint8_t xdag_remark_t[32];

struct xdag_field {
	union {
		struct {
			union {
				struct {
					uint64_t transport_header;
					uint64_t type;
					xtime_t time;
				};
				xdag_hashlow_t hash;
			};
			union {
				xdag_amount_t amount;
				xtime_t end_time;
			};
		};
		xdag_hash_t data;
		xdag_remark_t remark;
	};
};

struct xdag_block {
	struct xdag_field field[XDAG_BLOCK_FIELDS];
};

#define xdag_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

#define MAX_WAITING_MAIN        1
#define MAIN_START_AMOUNT       (1ll << 42)
#define MAIN_APOLLO_AMOUNT      (1ll << 39)
// 13500 is 10 days
// nmain = 834646 ,          at 2019-09-17 00:30:00
// nmain = 834646 + 4*13500, at 2019-10-27 00:30:00
#define MAIN_APOLLO_HIGHT       (834646 + 4*13500)
//#define MAIN_APOLLO_HIGHT       2  // for test
#define MAIN_BIG_PERIOD_LOG     21
#define MAX_LINKS               15
#define MAKE_BLOCK_PERIOD       13

#define CACHE            1
#define CACHE_MAX_SIZE        600000
#define CACHE_MAX_SAMPLES    100
#define ORPHAN_HASH_SIZE    2
#define MAX_ALLOWED_EXTRA    0x10000

struct block_backrefs;
struct orphan_block;
struct block_internal_index;

struct block_internal {
    //    union {
    //        struct ldus_rbtree node;
    //        struct block_internal_index *index;
    //    };
    xdag_hash_t hash;
    xdag_diff_t difficulty;
    xdag_amount_t amount, linkamount[MAX_LINKS], fee;
    xtime_t time;
    uint64_t storage_pos;
    union {
        xdag_hashlow_t ref;
        struct orphan_block *oref;
    };
    xdag_hashlow_t link[MAX_LINKS];
    //xdag_hashlow_t backrefs;
    atomic_uintptr_t remark;
    uint16_t flags, in_mask, n_our_key;
    uint8_t nlinks:4, max_diff_link:4, reserved;
};


struct block_internal_backref {
    xdag_hash_t hash;
    xdag_hashlow_t backref;
};

//struct block_internal_index {
//    struct ldus_rbtree node;
//    xdag_hash_t hash;
//    struct block_internal *bi;
//};

#define N_BACKREFS      (sizeof(struct block_internal) / sizeof(struct block_internal *) - 1)
        
#define ourprev link[MAX_LINKS - 2]
#define ournext link[MAX_LINKS - 1]

struct block_backrefs {
    struct block_internal *backrefs[N_BACKREFS];
    struct block_backrefs *next;
};

//struct cache_block {
//    struct ldus_rbtree node;
//    xdag_hash_t hash;
//    struct xdag_block block;
//    struct cache_block *next;
//};

struct orphan_block {
    struct block_internal* orphan_bi;
    struct orphan_block *next;
    struct orphan_block *prev;
    struct xdag_block block[0];
};

enum orphan_remove_actions {
    ORPHAN_REMOVE_NORMAL,
    ORPHAN_REMOVE_REUSE,
    ORPHAN_REMOVE_EXTRA
};

#define get_orphan_index(bi)      (!!((bi)->flags & BI_EXTRA))

#ifdef __cplusplus
extern "C" {
#endif

extern int g_bi_index_enable;

// start of regular block processing
extern int xdag_blocks_start(int mining_threads_count, int miner_address);

// checks and adds block to the storage. Returns non-zero value in case of error.
extern int xdag_add_block(struct xdag_block *b);

// returns our first block. If there is no blocks yet - the first block is created.
extern int xdag_get_our_block(xdag_hash_t hash);

// calls callback for each own block
extern int xdag_traverse_our_blocks(void *data,
	int (*callback)(void*, xdag_hash_t, xdag_amount_t, xtime_t, int));

// calls callback for each block
extern int xdag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
	xdag_amount_t amount, xtime_t time));

// create a new block
extern struct xdag_block* xdag_create_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// create and publish a block
extern int xdag_create_and_send_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// returns current balance for specified address or balance for all addresses if hash == 0
extern xdag_amount_t xdag_get_balance(xdag_hash_t hash);

// sets current balance for the specified address
extern int xdag_set_balance(xdag_hash_t hash, xdag_amount_t balance);

// calculates current supply by specified count of main blocks
extern xdag_amount_t xdag_get_supply(uint64_t nmain);

// returns position and time of block by hash; if block is extra and block != 0 also returns the whole block
extern int64_t xdag_get_block_pos(const xdag_hash_t hash, xtime_t *time, struct xdag_block *block);

// return state info string
extern const char* xdag_get_block_state_info(uint8_t flag);

// returns a number of key by hash of block or -1 if block is not ours
extern int xdag_get_key(xdag_hash_t hash);

// reinitialization of block processing
extern int xdag_blocks_reset(void);

// prints detailed information about block
extern int xdag_print_block_info(xdag_hash_t hash, FILE *out);

// prints list of N last main blocks
extern void xdag_list_main_blocks(int count, int print_only_addresses, FILE *out);

// prints list of N last blocks mined by current pool
extern void xdag_list_mined_blocks(int count, int include_non_payed, FILE *out);

// get all transactions of specified address, and return total number of transactions
extern int xdag_get_transactions(xdag_hash_t hash, void *data, int (*callback)(void*, int, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*));

// print orphan blocks
void xdag_list_orphan_blocks(int, FILE*);

// completes work with the blocks
void xdag_block_finish(void);

// add blocks with out sync for pool reload data
void *add_block_callback_nosync(void *block, void *data);
    
void *add_block_callback_sync(void *block, void *data);

void xdag_connect_block(struct xdag_block *b);


	
// get block info of specified address
extern int xdag_get_block_info(xdag_hash_t, void *, int (*)(void*, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*),
							void *, int (*)(void*, const char *, xdag_hash_t, xdag_amount_t));


#ifdef __cplusplus
};
#endif

#endif
