/* block processing, T13.654-T14.618 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "system.h"
#include "../ldus/rbtree.h"
#include "block.h"
#include "crypt.h"
#include "global.h"
#include "wallet.h"
#include "storage.h"
#include "transport.h"
#include "utils/log.h"
#include "init.h"
#include "sync.h"
#include "pool.h"
#include "miner.h"
#include "memory.h"
#include "address.h"
#include "commands.h"
#include "utils/utils.h"
#include "utils/moving_statistics/moving_average.h"
#include "mining_common.h"
#include "time.h"
#include "math.h"
#include "utils/atomic.h"
#include "utils/random.h"
#include "websocket/websocket.h"
#include "global.h"

#define MAX_WAITING_MAIN        1
#define MAIN_START_AMOUNT       (1ll << 42)
#define MAIN_APOLLO_AMOUNT      (1ll << 39)
// nmain = 976487, hash is WENN9ZgvXA+vNaslRLFQPgBKIbJVaMsu
//                         at 2019-12-30 18:01:35 UTC
//                         get this info from https://explorer.xdag.io/
//
// Apollo plans to upgrade on 2020-01-30 00:00:00 UTC
//
#define MAIN_APOLLO_HEIGHT           1017323
#define MAIN_APOLLO_TESTNET_HEIGHT   196250
#define MAIN_BIG_PERIOD_LOG     21
#define MAX_LINKS               15
#define MAKE_BLOCK_PERIOD       13

#define CACHE			1
#define CACHE_MAX_SIZE		600000
#define CACHE_MAX_SAMPLES	100
#define ORPHAN_HASH_SIZE	2
#define MAX_ALLOWED_EXTRA	0x10000

int g_block_production_on;
static pthread_mutex_t g_create_block_mutex = PTHREAD_MUTEX_INITIALIZER;
static xdag_amount_t g_balance = 0;
extern xtime_t g_time_limit;
static struct block_internal *volatile top_main_chain = 0, *volatile pretop_main_chain = 0;
static xdag_hash_t ourfirst_hash = {0};
static xdag_hash_t ourlast_hash = {0};
static pthread_mutex_t block_mutex;
static pthread_mutex_t rbtree_mutex;
static XDAG_BLOOM_FILTER *pBloom_filter = NULL;

//functions
void append_block_info(struct block_internal *bi);
//int32_t check_signature_out_cached(struct block_internal*, struct xdag_public_key*, const int, int32_t*, int32_t*);
int32_t check_signature_out(struct block_internal*, struct xdag_public_key*, const int);
static int32_t find_and_verify_signature_out(struct xdag_block*, struct xdag_public_key*, const int);
int do_mining(struct xdag_block *block, struct block_internal **pretop, xtime_t send_time);
int remove_orphan(xdag_hashlow_t);
void add_orphan(struct block_internal*, struct xdag_block*);
static inline size_t remark_acceptance(xdag_remark_t);
static int add_remark_bi(struct block_internal*, xdag_remark_t);
static inline const char* get_remark(struct block_internal*);
static int load_remark(struct block_internal*);
//static void order_ourblocks_by_amount(struct block_internal *bi);
static inline void add_ourblock(struct block_internal *nodeBlock);
extern void *sync_thread(void *arg);

static void log_block(const char *mess, xdag_hash_t h, xtime_t t, uint64_t pos)
{
	/* Do not log blocks as we are loading from local storage */
    if(g_xdag_state != XDAG_STATE_LOAD) {
        xdag_info("%s: %016llx%016llx%016llx%016llx t=%llx pos=%llx", mess,
            ((uint64_t*)h)[3], ((uint64_t*)h)[2], ((uint64_t*)h)[1], ((uint64_t*)h)[0], t, pos);
    }
}

static inline void accept_amount(struct block_internal *bi, xdag_amount_t sum)
{
	if (!sum) {
		return;
	}

	bi->amount += sum;
	if (bi->flags & BI_OURS) {
		g_balance += sum;
		//order_ourblocks_by_amount(bi);
	}
}

static uint64_t apply_block(struct block_internal *bi)
{
    xdag_amount_t sum_in = 0;
    xdag_amount_t sum_out = 0;

	if (bi->flags & BI_MAIN_REF) {
		return -1l;
	}

	bi->flags |= BI_MAIN_REF;
	for (int i = 0; i < bi->nlinks; ++i) {
		struct block_internal lbi;
        if(!xdag_rsdb_get_bi(bi->link[i],&lbi)){
			xdag_amount_t ref_amount = apply_block(&lbi);
			if (ref_amount == -1l) {
				continue;
			}
			memcpy(lbi.ref, bi->hash, sizeof(xdag_hashlow_t));
			if (bi->amount + ref_amount >= bi->amount) {
				accept_amount(bi, ref_amount);
			}
        }
	}

    sum_in = 0;
    sum_out = bi->fee;

	for (int i = 0; i < bi->nlinks; ++i) {
		struct block_internal lbi;
		if(!xdag_rsdb_get_bi(bi->link[i],&lbi)) {
			if (1 << i & bi->in_mask) {
				if (lbi.amount < bi->linkamount[i]) {
					return 0;
				}
				if (sum_in + bi->linkamount[i] < sum_in) {
					return 0;
				}
				sum_in += bi->linkamount[i];
			} else {
				if (sum_out + bi->linkamount[i] < sum_out) {
					return 0;
				}
				sum_out += bi->linkamount[i];
			}
		}
	}

	if (sum_in + bi->amount < sum_in || sum_in + bi->amount < sum_out) {
		return 0;
	}

	for (int i = 0; i < bi->nlinks; ++i) {
		struct block_internal lbi;
		if(!xdag_rsdb_get_bi(bi->link[i],&lbi)) {
			if (1 << i & bi->in_mask) {
				accept_amount(&lbi, (xdag_amount_t) 0 - bi->linkamount[i]);
			} else {
				accept_amount(&lbi, bi->linkamount[i]);
			}
		}
	}

    accept_amount(bi, sum_in - sum_out);
	bi->flags |= BI_APPLIED;
    xdag_rsdb_put_bi(bi);
	append_block_info(bi); //TODO: figure out how to detect when the block is rejected.
	return bi->fee;
}

static uint64_t unapply_block(struct block_internal* bi)
{
    int retcode = 0;
	if (bi->flags & BI_APPLIED) {
		xdag_amount_t sum = bi->fee;

		for (int i = 0; i < bi->nlinks; ++i) {
            struct block_internal lbi;
            if(!xdag_rsdb_get_bi(bi->link[i],&lbi)) {
                if (1 << i & bi->in_mask) {
                    accept_amount(&lbi, bi->linkamount[i]);
                    sum -= bi->linkamount[i];
                } else {
                    accept_amount(&lbi, (xdag_amount_t)0 - bi->linkamount[i]);
                    sum += bi->linkamount[i];
                }
            }
		}

		accept_amount(bi, sum);
		bi->flags &= ~BI_APPLIED;
	}

	bi->flags &= ~BI_MAIN_REF;
    memset(bi->ref, 0, sizeof(xdag_hashlow_t));

	for (int i = 0; i < bi->nlinks; ++i) {
        struct block_internal lbi;
        if(!xdag_rsdb_get_bi(bi->link[i],&lbi)) {
            if (memcmp(lbi.ref, bi->hash, sizeof(xdag_hashlow_t)) == 0 && lbi.flags & BI_MAIN_REF) {
                accept_amount(bi, unapply_block(&lbi));
            }
        }
	}
    xdag_rsdb_put_bi(bi);
	return (xdag_amount_t)0 - bi->fee;
}

static xdag_amount_t get_start_amount_with_time(xdag_time_t time, uint64_t nmain) {
    xdag_amount_t start_amount = 0;
    uint64_t fork_height = g_xdag_testnet ? MAIN_APOLLO_TESTNET_HEIGHT:MAIN_APOLLO_HEIGHT;
    if(nmain >= fork_height) {
        if(g_apollo_fork_time == 0) {
            g_apollo_fork_time = time;
        }
        start_amount = MAIN_APOLLO_AMOUNT;
    } else {
        start_amount = MAIN_START_AMOUNT;
    }
    return start_amount;
}

static xdag_amount_t get_start_amount(uint64_t nmain) {
    xdag_amount_t start_amount = 0;
    uint64_t fork_height = g_xdag_testnet ? MAIN_APOLLO_TESTNET_HEIGHT:MAIN_APOLLO_HEIGHT;
    if(nmain >= fork_height) {
        start_amount = MAIN_APOLLO_AMOUNT;
    } else {
        start_amount = MAIN_START_AMOUNT;
    }
    return start_amount;
}

static xdag_amount_t get_amount(xdag_time_t time, uint64_t nmain) {
    xdag_amount_t amount = 0;
    xdag_amount_t start_amount = 0;
    
    start_amount = get_start_amount_with_time(time, nmain);
    amount = start_amount >> (nmain >> MAIN_BIG_PERIOD_LOG);
    return amount;
}

xdag_amount_t xdag_get_supply(uint64_t nmain)
{
    xdag_amount_t res = 0, amount = get_start_amount(nmain);
    uint64_t current_nmain = nmain;
    while (current_nmain >> MAIN_BIG_PERIOD_LOG) {
        res += (1l << MAIN_BIG_PERIOD_LOG) * amount;
        current_nmain -= 1l << MAIN_BIG_PERIOD_LOG;
        amount >>= 1;
    }
    res += current_nmain * amount;
    uint64_t fork_height = g_xdag_testnet?MAIN_APOLLO_TESTNET_HEIGHT:MAIN_APOLLO_HEIGHT;
    if(nmain >= fork_height) {
        // add before apollo amount
        res += (fork_height - 1) * (MAIN_START_AMOUNT - MAIN_APOLLO_AMOUNT);
    }
    return res;
}

static void set_main(struct block_internal *m)
{
    xdag_amount_t amount = 0;
    g_xdag_stats.nmain++;
    if (g_xdag_stats.nmain > g_xdag_stats.total_nmain) {
        g_xdag_stats.total_nmain = g_xdag_stats.nmain;
    }
    amount = get_amount(m->time, g_xdag_stats.nmain + 1);
	m->flags |= BI_MAIN;
	accept_amount(m, amount);
	accept_amount(m, apply_block(m));
    memcpy(m->ref, m->hash, sizeof(xdag_hashlow_t));
    xdag_rsdb_put_bi(m);
	log_block((m->flags & BI_OURS ? "MAIN +" : "MAIN  "), m->hash, m->time, m->storage_pos);
}

static void unset_main(struct block_internal *m)
{
    xdag_amount_t amount = 0;
	g_xdag_stats.nmain--;
	g_xdag_stats.total_nmain--;
	amount = get_amount(m->time, g_xdag_stats.nmain);
	m->flags &= ~BI_MAIN;
	accept_amount(m, (xdag_amount_t)0 - amount);
	accept_amount(m, unapply_block(m));
    xdag_rsdb_put_bi(m);
	log_block("UNMAIN", m->hash, m->time, m->storage_pos);
}

static void check_new_main(void)
{
//    struct block_internal *b, *p = 0;
//    xdag_hash_t b_hash = {0};
    int i = 0;

//    for (b = top_main_chain, i = 0;
//         b && !(b->flags & BI_MAIN);
//         b = b->link[b->max_diff_link])
//    {
//        if (b->flags & BI_MAIN_CHAIN) {
//            p = b;
//            ++i;
//        }
//    }
    struct block_internal bi;
    struct block_internal pre_bi;
    XDAG_RSDB_OP_TYPE retcode = 0;
    struct block_internal *p = NULL;
    memset(&bi, 0, sizeof(struct block_internal));
    memset(&pre_bi, 0, sizeof(struct block_internal));
    if(top_main_chain ) memcpy(&bi, top_main_chain, sizeof(struct block_internal));
    for(i = 0;
        !retcode && !(bi.flags & BI_MAIN);
        retcode = xdag_rsdb_get_bi(pre_bi.link[pre_bi.max_diff_link], &bi))
    {
        memcpy(&pre_bi, &bi, sizeof(struct block_internal));
        if(bi.flags & BI_MAIN_CHAIN) {
            p = &pre_bi;
            ++i;
        }
    }

    if (p && (p->flags & BI_REF) && i > MAX_WAITING_MAIN && xdag_get_xtimestamp() >= p->time + 2 * 1024) {
        set_main(p);
    }
}

static void unwind_main(struct block_internal *bi)
{
    //    for (struct block_internal *t = top_main_chain; t && t != b; t = t->link[t->max_diff_link]) {
//        t->flags &= ~BI_MAIN_CHAIN;
//        if (t->flags & BI_MAIN) {
//            unset_main(t);
//        }
//    }
    struct block_internal tbi;
    struct block_internal tpre_bi;
    XDAG_RSDB_OP_TYPE retcode = 1;
    memset(&tbi, 0, sizeof(struct block_internal));
    memset(&tpre_bi, 0, sizeof(struct block_internal));
    if(top_main_chain) {
        retcode = 1;
        memcpy(&tbi, top_main_chain, sizeof(struct block_internal));
    }
    for (;!retcode && memcmp(tbi.hash, bi->hash, sizeof(xdag_hashlow_t));
         retcode = xdag_rsdb_get_bi(tpre_bi.link[tpre_bi.max_diff_link], &tbi))
    {
        tbi.flags &= ~BI_MAIN_CHAIN;
        xdag_rsdb_put_bi(&tbi);
        if (tbi.flags & BI_MAIN) {
            unset_main(&tbi);
        }
        memcpy(&tpre_bi, &tbi, sizeof(struct block_internal));
    }
}

static inline void hash_for_signature(struct xdag_block b[2], const struct xdag_public_key *key, xdag_hash_t hash)
{
	memcpy((uint8_t*)(b + 1) + 1, (void*)((uintptr_t)key->pub & ~1l), sizeof(xdag_hash_t));

	*(uint8_t*)(b + 1) = ((uintptr_t)key->pub & 1) | 0x02;

	xdag_hash(b, sizeof(struct xdag_block) + sizeof(xdag_hash_t) + 1, hash);

//    xdag_debug("Hash  : hash=[%s] data=[%s]", xdag_log_hash(hash),
//        xdag_log_array(b, sizeof(struct xdag_block) + sizeof(xdag_hash_t) + 1));
}

/*
 * cache the sign for block_internal for next time spent to verify
 */
//static int cache_bi_sign(const struct xdag_block *b, int signo_r, struct block_internal* bi)
//{
//    struct xdag_block buf[2];
//    int i, signo_s = -1;
//
//    memcpy(buf, b, sizeof(struct xdag_block));
//    for(i = signo_r; i < XDAG_BLOCK_FIELDS; ++i) {
//        if(xdag_type(b, i) == XDAG_FIELD_SIGN_IN || xdag_type(b, i) == XDAG_FIELD_SIGN_OUT) {
//            memset(&buf[0].field[i], 0, sizeof(struct xdag_field));
//            if(i > signo_r && signo_s < 0 && xdag_type(b, i) == xdag_type(b, signo_r)) {
//                signo_s = i;
//            }
//        }
//    }
//    if(signo_s >= 0) {
//        memcpy(utxo->sign_r, b->field[signo_r].data, sizeof(xdag_hash_t));
//        memcpy(utxo->sign_s, b->field[signo_s].data, sizeof(xdag_hash_t));
//        return 0;
//    }
//    return -1;
//}

// returns a number of public key from 'keys' array with lengh 'keysLength', which conforms to the signature starting from field signo_r of the block b
// returns -1 if nothing is found
static int valid_signature(const struct xdag_block *b, int signo_r, int keysLength, struct xdag_public_key *keys)
{
	struct xdag_block buf[2];
	xdag_hash_t hash;
	int i, signo_s = -1;

	memcpy(buf, b, sizeof(struct xdag_block));

	for(i = signo_r; i < XDAG_BLOCK_FIELDS; ++i) {
		if(xdag_type(b, i) == XDAG_FIELD_SIGN_IN || xdag_type(b, i) == XDAG_FIELD_SIGN_OUT) {
			memset(&buf[0].field[i], 0, sizeof(struct xdag_field));
			if(i > signo_r && signo_s < 0 && xdag_type(b, i) == xdag_type(b, signo_r)) {
				signo_s = i;
			}
		}
	}

	if(signo_s >= 0) {
		for(i = 0; i < keysLength; ++i) {
			hash_for_signature(buf, keys + i, hash);

#if USE_OPTIMIZED_EC == 1
			if(!xdag_verify_signature_optimized_ec(keys[i].pub, hash, b->field[signo_r].data, b->field[signo_s].data)) {
#elif USE_OPTIMIZED_EC == 2
			int res1 = !xdag_verify_signature_optimized_ec(keys[i].pub, hash, b->field[signo_r].data, b->field[signo_s].data);
			int res2 = !xdag_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data);
			if(res1 != res2) {
//                xdag_warn("Different result between openssl and secp256k1: res openssl=%2d res secp256k1=%2d key parity bit = %ld key=[%s] hash=[%s] r=[%s], s=[%s]",
//                    res2, res1, ((uintptr_t)keys[i].pub & 1), xdag_log_hash((uint64_t*)((uintptr_t)keys[i].pub & ~1l)),
//                    xdag_log_hash(hash), xdag_log_hash(b->field[signo_r].data), xdag_log_hash(b->field[signo_s].data));
			}
			if(res2) {
#else
			if(!xdag_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data)) {
#endif
				return i;
			}
		}
	}

	return -1;
}

#define set_pretop(b) if ((b) && MAIN_TIME((b)->time) < MAIN_TIME(timestamp) && \
		(!pretop_main_chain || xdag_diff_gt((b)->difficulty, pretop_main_chain->difficulty))) { \
		pretop_main_chain = (b); \
		log_block("Pretop", (b)->hash, (b)->time, (b)->storage_pos); \
}

static int check_block_exist(xdag_hashlow_t hash)
{
//    struct block_internal bi;
//    struct xdag_block xb;
//    if(!xdag_rsdb_get_bi(hash, &bi)){
//        return 1;
//    }
//
//    if(!xdag_rsdb_get_orpblock(hash, &xb)){
//        return 1;
//    }
    if(pBloom_filter) {
        if(xdag_bloom_filter_wasadded(pBloom_filter, (char*)hash))
        {
            return 1;
        }
    } else {
        pBloom_filter = xdag_bloom_filter_new();
    }
    return 0;
}

static int check_block_header(struct xdag_block *newBlock, int *pi)
{
    if(xdag_type(newBlock, 0) != g_block_header_type) {
        *pi = xdag_type(newBlock, 0);
        return 1;
    }
    return 0;
}

static int check_block_time(struct xdag_block *newBlock, xtime_t limit, int *pi)
{
    const xtime_t timestamp = xdag_get_xtimestamp();
    xtime_t t = newBlock->field[0].time;
    if(t > timestamp + MAIN_CHAIN_PERIOD / 4 || t < XDAG_TEST_ERA
		|| (limit && timestamp - t > limit)) {
        *pi = 0;
		return 2;
	}
    return 0;
}

/* checks and adds a new block to the storage
 * returns:
 *		>0 = block was added
 *		0  = block exists
 *		<0 = error
 */
static int add_block_nolock(struct xdag_block *newBlock, xtime_t limit)
{
    const xtime_t timestamp = xdag_get_xtimestamp();
    uint64_t sum_in = 0, sum_out = 0, *psum = NULL;
	const uint64_t transportHeader = newBlock->field[0].transport_header;
	struct xdag_public_key public_keys[16], *our_keys = 0;
	int i = 0, j = 0;
	int keysCount = 0, ourKeysCount = 0;
	int signInCount = 0, signOutCount = 0;
	int signinmask = 0, signoutmask = 0;
	int inmask = 0, outmask = 0, remark_index = 0;
	int verified_keys_mask = 0, err = 0, type = 0;
    struct block_internal tmpNodeBlock, blockRef, blockRef0, *pt = NULL;
    struct block_internal *nodeBlock = NULL;
	struct block_internal blockRefs[XDAG_BLOCK_FIELDS-1];
	xdag_diff_t diff0, diff;
	
	memset(blockRefs,0, sizeof(blockRefs));
    memset(&tmpNodeBlock, 0, sizeof(struct block_internal));
    newBlock->field[0].transport_header = 0;
    xdag_hash(newBlock, sizeof(struct xdag_block), tmpNodeBlock.hash);

    if(check_block_exist(tmpNodeBlock.hash))
        return 0;
    if((err = check_block_header(newBlock, &i))) {
        goto end;
    }
    if((err = check_block_time(newBlock, limit, &i))) {
        goto end;
    }

	tmpNodeBlock.time = newBlock->field[0].time;

	for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		switch((type = xdag_type(newBlock, i))) {
			case XDAG_FIELD_NONCE:
				break;
			case XDAG_FIELD_IN:
				inmask |= 1 << i;
				break;
			case XDAG_FIELD_OUT:
				outmask |= 1 << i;
				break;
			case XDAG_FIELD_SIGN_IN:
				if(++signInCount & 1) {
					signinmask |= 1 << i;
				}
				break;
			case XDAG_FIELD_SIGN_OUT:
				if(++signOutCount & 1) {
					signoutmask |= 1 << i;
				}
				break;
			case XDAG_FIELD_PUBLIC_KEY_0:
			case XDAG_FIELD_PUBLIC_KEY_1:
				if((public_keys[keysCount].key = xdag_public_to_key(newBlock->field[i].data, type - XDAG_FIELD_PUBLIC_KEY_0))) {
					public_keys[keysCount++].pub = (uint64_t*)((uintptr_t)&newBlock->field[i].data | (type - XDAG_FIELD_PUBLIC_KEY_0));
				}
				break;

			case XDAG_FIELD_REMARK:
				tmpNodeBlock.flags |= BI_REMARK;
				remark_index = i;
				break;
			case XDAG_FIELD_RESERVE1:
			case XDAG_FIELD_RESERVE2:
			case XDAG_FIELD_RESERVE3:
			case XDAG_FIELD_RESERVE4:
			case XDAG_FIELD_RESERVE5:
			case XDAG_FIELD_RESERVE6:
				break;
			default:
				err = 3;
				goto end;
		}
	}

	if(is_wallet()) {
		outmask = 0;
	}

	if(signOutCount & 1) {
		i = signOutCount;
		err = 4;
		goto end;
	}

	/* check remark */
	if(tmpNodeBlock.flags & BI_REMARK) {
		if(!remark_acceptance(newBlock->field[remark_index].remark)) {
			err = 0xE;
			goto end;
		}
	}

	/* if not read from storage and timestamp is ...ffff and last field is nonce then the block is extra */
	if (is_pool() && (transportHeader & (sizeof(struct xdag_block) - 1))
			&& (tmpNodeBlock.time & (MAIN_CHAIN_PERIOD - 1)) == (MAIN_CHAIN_PERIOD - 1)
			&& (signinmask & 1 << (XDAG_BLOCK_FIELDS - 1))) {
		tmpNodeBlock.flags |= BI_EXTRA;
	}

    // check ref block
    for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
        if(1 << i & (inmask | outmask)) {
            if(xdag_rsdb_get_bi(newBlock->field[i].hash, &blockRefs[i-1])){
                err = 5;
                goto end;
            }
            if(blockRefs[i-1].time >= tmpNodeBlock.time) {
                err = 6;
                goto end;
            }
            if(tmpNodeBlock.nlinks >= MAX_LINKS) {
                err = 7;
                goto end;
            }
        }
    }

	if(is_pool()) {
		check_new_main();
	}

	if(signOutCount) {
		our_keys = xdag_wallet_our_keys(&ourKeysCount);
	}
    
    // check sign
	for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
        
		if(1 << i & (signinmask | signoutmask)) {
            
			int keyNumber = valid_signature(newBlock, i, keysCount, public_keys);
            
			if(keyNumber >= 0) {
				verified_keys_mask |= 1 << keyNumber;
			}
             
			if(1 << i & signoutmask && !(tmpNodeBlock.flags & BI_OURS) && (keyNumber = valid_signature(newBlock, i, ourKeysCount, our_keys)) >= 0) {
				tmpNodeBlock.flags |= BI_OURS;
				tmpNodeBlock.n_our_key = keyNumber;
			}
            
		}
        
	}

	for(i = j = 0; i < keysCount; ++i) {
		if(1 << i & verified_keys_mask) {
			if(i != j) {
				xdag_free_key(public_keys[j].key);
			}
			memcpy(public_keys + j++, public_keys + i, sizeof(struct xdag_public_key));
		}
	}

	keysCount = j;
	tmpNodeBlock.difficulty = diff0 = xdag_hash_difficulty(tmpNodeBlock.hash);
	sum_out += newBlock->field[0].amount;
	tmpNodeBlock.fee = newBlock->field[0].amount;
	if (tmpNodeBlock.fee) {
		tmpNodeBlock.flags &= ~BI_EXTRA;
	}

    // refactor to connect_method like bitcoin
	for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (inmask | outmask)) {
            struct block_internal *tmpblockRef = &blockRefs[i-1];
			if(1 << i & inmask) {
				if(newBlock->field[i].amount) {
					int32_t res = 1;
                    res = check_signature_out(tmpblockRef, public_keys, keysCount);
					if(res) {
						err = res;
						goto end;
					}

				}
				psum = &sum_in;
				tmpNodeBlock.in_mask |= 1 << tmpNodeBlock.nlinks;
			} else {
				psum = &sum_out;
			}

			if (newBlock->field[i].amount) {
				tmpNodeBlock.flags &= ~BI_EXTRA;
			}

			if(*psum + newBlock->field[i].amount < *psum) {
				err = 0xA;
				goto end;
			}

			*psum += newBlock->field[i].amount;
			//tmpNodeBlock.link[tmpNodeBlock.nlinks] = blockRef->hash;
            memcpy(tmpNodeBlock.link[tmpNodeBlock.nlinks], blockRefs[i-1].hash, sizeof(xdag_hashlow_t));
			tmpNodeBlock.linkamount[tmpNodeBlock.nlinks] = newBlock->field[i].amount;

			// check ref block time
			if(MAIN_TIME(blockRefs[i-1].time) < MAIN_TIME(tmpNodeBlock.time)) {
				diff = xdag_diff_add(diff0, blockRefs[i-1].difficulty);
			} else {
				diff = blockRefs[i-1].difficulty;
                struct block_internal tmp;
                int retcode = 0;
                memcpy(&tmp, &blockRefs[i-1], sizeof(struct block_internal));
				while(!retcode && MAIN_TIME(tmp.time) == MAIN_TIME(tmpNodeBlock.time)) {
                    retcode = xdag_rsdb_get_bi(tmp.link[tmp.max_diff_link], &tmp);
				}
				if(xdag_diff_gt(xdag_diff_add(diff0, tmp.difficulty), diff)) {
					diff = xdag_diff_add(diff0, tmp.difficulty);
				}
			}

			if(xdag_diff_gt(diff, tmpNodeBlock.difficulty)) {
				tmpNodeBlock.difficulty = diff;
				tmpNodeBlock.max_diff_link = tmpNodeBlock.nlinks;
			}
			tmpNodeBlock.nlinks++;
		}
	}

	if(tmpNodeBlock.in_mask ? sum_in < sum_out : sum_out != newBlock->field[0].amount) {
		err = 0xB;
		goto end;
	}

	if(!(transportHeader & (sizeof(struct xdag_block) - 1))) {
		tmpNodeBlock.storage_pos = transportHeader;
	} else if (!(tmpNodeBlock.flags & BI_EXTRA)) {
		tmpNodeBlock.storage_pos = xdag_storage_save(newBlock);
        //xdag_info("!(tmpNodeBlock.flags & BI_EXTRA)->save: %016llx%016llx%016llx%016llx", tmpNodeBlock.hash[3], tmpNodeBlock.hash[2], tmpNodeBlock.hash[1], tmpNodeBlock.hash[0]);
	} else {
		tmpNodeBlock.storage_pos = -2l;
	}

    nodeBlock = calloc(sizeof(struct block_internal), 1);
    if(!nodeBlock) {
        err = 0xC;
        goto end;
    }

	memcpy(nodeBlock, &tmpNodeBlock, sizeof(struct block_internal));
	//atomic_init_uintptr(&nodeBlock->backrefs, (uintptr_t)NULL);
    //memset(nodeBlock->backrefs, 0, sizeof(xdag_hashlow_t));

	if(nodeBlock->flags & BI_REMARK) {
        xdag_rsdb_put_remark(nodeBlock, newBlock->field[remark_index].remark);
		atomic_init_uintptr(&nodeBlock->remark, (uintptr_t)NULL);
	}

    if(!xdag_rsdb_put_bi(nodeBlock)) {
        //xdag_info("xdag_rsdb_put_bi_835: hash(%016llx%016llx%016llx%016llx),pos=%llx,flags=%d", nodeBlock->hash[3], nodeBlock->hash[2], nodeBlock->hash[1], nodeBlock->hash[0], nodeBlock->storage_pos, nodeBlock->flags);
        xdag_bloom_filter_add(pBloom_filter,(char*)nodeBlock->hash);
        g_xdag_stats.nblocks++;
    } else {
        err = 0xD;
        goto end;
    }

	if(g_xdag_stats.nblocks > g_xdag_stats.total_nblocks) {
		g_xdag_stats.total_nblocks = g_xdag_stats.nblocks;
	}

    set_pretop(nodeBlock);
	set_pretop(top_main_chain);

	if(xdag_diff_gt(tmpNodeBlock.difficulty, g_xdag_stats.difficulty)) {
		
		if(g_xdag_state != XDAG_STATE_LOAD)
        {
			xdag_info("Diff  : %llx%016llx (+%llx%016llx)", xdag_diff_args(tmpNodeBlock.difficulty), xdag_diff_args(diff0));
        }

//        for(blockRef = nodeBlock, blockRef0 = 0; blockRef && !(blockRef->flags & BI_MAIN_CHAIN); blockRef = blockRef->link[blockRef->max_diff_link]) {
//            if((!blockRef->link[blockRef->max_diff_link] || xdag_diff_gt(blockRef->difficulty, blockRef->link[blockRef->max_diff_link]->difficulty))
//               && (!blockRef0 || MAIN_TIME(blockRef0->time) > MAIN_TIME(blockRef->time))) {
//                blockRef->flags |= BI_MAIN_CHAIN;
//                blockRef0 = blockRef;
//            }
//        }

        int bf_retcode = 1;
		int bf0_exist = 0;
        if(nodeBlock) {
            memcpy(&blockRef, nodeBlock, sizeof(struct block_internal));
            memset(&blockRef0, 0, sizeof(struct block_internal));
            bf_retcode = 0;
        }
        struct block_internal pbf;
		for(;!bf_retcode && !(blockRef.flags & BI_MAIN_CHAIN);
		      bf_retcode = xdag_rsdb_get_bi(pbf.link[pbf.max_diff_link], &blockRef))
        {
            struct block_internal maxDiffLink;
			if((!xdag_rsdb_get_bi(blockRef.link[blockRef.max_diff_link], &maxDiffLink) || xdag_diff_gt(blockRef.difficulty, maxDiffLink.difficulty))
				&& (!bf0_exist || MAIN_TIME(blockRef0.time) > MAIN_TIME(blockRef.time)))
            {
				blockRef.flags |= BI_MAIN_CHAIN;
                memcpy(&blockRef0, &blockRef, sizeof(struct block_internal));
                bf0_exist = 1;
                //xdag_info("xdag_rsdb_put_bi_882: hash(%016llx%016llx%016llx%016llx),pos=%llx,flags=%d", blockRef->hash[3], blockRef->hash[2], blockRef->hash[1], blockRef->hash[0], blockRef->storage_pos, blockRef->flags);
				xdag_rsdb_put_bi(&blockRef);
			}
			memcpy(&pbf, &blockRef, sizeof(struct block_internal));
		}

//        if(blockRef && blockRef0 && blockRef != blockRef0 && MAIN_TIME(blockRef->time) == MAIN_TIME(blockRef0->time)) {
//            blockRef = blockRef->link[blockRef->max_diff_link];
//        }

		if(!bf_retcode && bf0_exist && memcmp(blockRef.hash, blockRef0.hash, sizeof(xdag_hashlow_t)) &&
           MAIN_TIME(blockRef.time) == MAIN_TIME(blockRef0.time))
        {
            xdag_rsdb_get_bi(blockRef.link[blockRef.max_diff_link], &blockRef);
		}
//        unwind_main(blockRef);
//        top_main_chain = nodeBlock;
//        g_xdag_stats.difficulty = tmpNodeBlock.difficulty;

		unwind_main(&blockRef);
        if(top_main_chain) {
            free(top_main_chain);
            top_main_chain = NULL;
        }
		top_main_chain = nodeBlock;
        memcpy(g_top_main_chain_hash, nodeBlock->hash, sizeof(g_top_main_chain_hash));
        xdag_rsdb_put_setting(SETTING_TOP_MAIN_HASH, (const char*)g_top_main_chain_hash, sizeof(g_top_main_chain_hash));
		g_xdag_stats.difficulty = tmpNodeBlock.difficulty;

		if(xdag_diff_gt(g_xdag_stats.difficulty, g_xdag_stats.max_difficulty)) {
			g_xdag_stats.max_difficulty = g_xdag_stats.difficulty;
		}
		err = -1;
	} else if (tmpNodeBlock.flags & BI_EXTRA) {
		err = 0;
	} else {
		err = -1;
	}

    // put to rocksdb
	if(tmpNodeBlock.flags & BI_OURS) {
        add_ourblock(nodeBlock);
	}

    for(i = 0; i < tmpNodeBlock.nlinks; ++i) {
        remove_orphan(tmpNodeBlock.link[i]);
    }
	add_orphan(nodeBlock, newBlock);
    //xdag_info("add_orphan: hash(%016llx%016llx%016llx%016llx),pos=%llx,flags=%d", nodeBlock->hash[3], nodeBlock->hash[2], nodeBlock->hash[1], nodeBlock->hash[0], nodeBlock->storage_pos, nodeBlock->flags);
	//log_block((tmpNodeBlock.flags & BI_OURS ? "Good +" : "Good  "), tmpNodeBlock.hash, tmpNodeBlock.time, tmpNodeBlock.storage_pos);

	i = MAIN_TIME(nodeBlock->time) & (HASHRATE_LAST_MAX_TIME - 1);
	if(MAIN_TIME(nodeBlock->time) > MAIN_TIME(g_xdag_extstats.hashrate_last_time)) {
		memset(g_xdag_extstats.hashrate_total + i, 0, sizeof(xdag_diff_t));
		memset(g_xdag_extstats.hashrate_ours + i, 0, sizeof(xdag_diff_t));
		g_xdag_extstats.hashrate_last_time = nodeBlock->time;
	}

	if(xdag_diff_gt(diff0, g_xdag_extstats.hashrate_total[i])) {
		g_xdag_extstats.hashrate_total[i] = diff0;
	}

	if(tmpNodeBlock.flags & BI_OURS && xdag_diff_gt(diff0, g_xdag_extstats.hashrate_ours[i])) {
		g_xdag_extstats.hashrate_ours[i] = diff0;
	}
    xdag_rsdb_put_stats();
    xdag_rsdb_put_extstats();
end:
	for(j = 0; j < keysCount; ++j) {
		xdag_free_key(public_keys[j].key);
	}

	if(err > 0) {
		char buf[32] = {0};
		err |= i << 4;
		sprintf(buf, "Err %2x", err & 0xff);
		//log_block(buf, tmpNodeBlock.hash, tmpNodeBlock.time, transportHeader);
	}
    // free nodeBlock
    if(nodeBlock != NULL && nodeBlock != top_main_chain) {
        free(nodeBlock);
    }
	return -err;
}

void *add_block_callback_nosync(void *block, void *data)
{
    unsigned *i = (unsigned *)data;
    xdag_add_block((struct xdag_block *)block);
    if(!(++*i % 10000)) xdag_info("nosync add blocks: %u\n", *i);
    return 0;
}
        
void *add_block_callback_sync(void *block, void *data)
{
	struct xdag_block *b = (struct xdag_block *)block;
	xtime_t *t = (xtime_t*)data;
	int res;

	pthread_mutex_lock(&block_mutex);

	if(*t < XDAG_ERA) {
		(res = add_block_nolock(b, *t));
	} else if((res = add_block_nolock(b, 0)) >= 0 && b->field[0].time > *t) {
		*t = b->field[0].time;
	}

	pthread_mutex_unlock(&block_mutex);

	if(res >= 0 && is_pool() && g_xdag_state != XDAG_STATE_LOAD && g_xdag_state != XDAG_STATE_INIT) {
		xdag_sync_pop_block(b);
	}

	return 0;
}

/* checks and adds block to the storage. Returns non-zero value in case of error. */
int xdag_add_block(struct xdag_block *b)
{
	pthread_mutex_lock(&block_mutex);
	int res = add_block_nolock(b, g_time_limit);
	pthread_mutex_unlock(&block_mutex);

	return res;
}

#define setfld(fldtype, src, hashtype) ( \
		block[0].field[0].type |= (uint64_t)(fldtype) << (i << 2), \
			memcpy(&block[0].field[i++], (void*)(src), sizeof(hashtype)) \
		)

#define pretop_block() (top_main_chain && MAIN_TIME(top_main_chain->time) == MAIN_TIME(send_time) ? pretop_main_chain : top_main_chain)

/* create a new block
 * The first 'ninput' field 'fields' contains the addresses of the inputs and the corresponding quantity of XDAG,
 * in the following 'noutput' fields similarly - outputs, fee; send_time (time of sending the block);
 * if it is greater than the current one, then the mining is performed to generate the most optimal hash
 */
struct xdag_block* xdag_create_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark,
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result)
{
	pthread_mutex_lock(&g_create_block_mutex);
	struct xdag_block block[2];
	memset(block,0, sizeof(block));
	int i, j, res, mining, defkeynum, keysnum[XDAG_BLOCK_FIELDS], nkeys, nkeysnum = 0, outsigkeyind = -1, has_pool_tag = 0;
	struct xdag_public_key *defkey = xdag_wallet_default_key(&defkeynum), *keys = xdag_wallet_our_keys(&nkeys), *key;
    xdag_hash_t signatureHash = {0};
    xdag_hash_t newBlockHash = {0};
	struct block_internal ref, *pretop = pretop_block();

	for (i = 0; i < inputsCount; ++i) {
        if(!xdag_rsdb_get_bi(fields[i].hash, &ref) || !(ref.flags & BI_OURS))
        {
            pthread_mutex_unlock(&g_create_block_mutex);
            return NULL;
        }

		for (j = 0; j < nkeysnum && ref.n_our_key != keysnum[j]; ++j);

		if (j == nkeysnum) {
			if (outsigkeyind < 0 && ref.n_our_key == defkeynum) {
				outsigkeyind = nkeysnum;
			}
			keysnum[nkeysnum++] = ref.n_our_key;
		}
	}
	pthread_mutex_unlock(&g_create_block_mutex);

	int res0 = 1 + inputsCount + outputsCount + hasRemark + 3 * nkeysnum + (outsigkeyind < 0 ? 2 : 0);

	if (res0 > XDAG_BLOCK_FIELDS) {
		xdag_err("create block failed, exceed max number of fields.");
		return NULL;
	}

	if (!send_time) {
		send_time = xdag_get_xtimestamp();
		mining = 0;
	} else {
		mining = (send_time > xdag_get_xtimestamp() && res0 + 1 <= XDAG_BLOCK_FIELDS);
	}

	res0 += mining;

#if REMARK_ENABLED
	/* reserve field for pool tag in generated main block */
	has_pool_tag = g_pool_has_tag;
	res0 += has_pool_tag * mining;
#endif

 begin:
	res = res0;
	memset(block, 0, sizeof(struct xdag_block));
	i = 1;
	block[0].field[0].type = g_block_header_type | (mining ? (uint64_t)XDAG_FIELD_SIGN_IN << ((XDAG_BLOCK_FIELDS - 1) * 4) : 0);
	block[0].field[0].time = send_time;
	block[0].field[0].amount = fee;


    struct block_internal ourfirst;
    int retcode = xdag_rsdb_get_ourbi(ourfirst_hash, &ourfirst);

	if (is_wallet()) {
		pthread_mutex_lock(&g_create_block_mutex);
		if (res < XDAG_BLOCK_FIELDS && !retcode) {
			setfld(XDAG_FIELD_OUT, ourfirst.hash, xdag_hashlow_t);
			res++;
		}
		pthread_mutex_unlock(&g_create_block_mutex);
	} else {
		pthread_mutex_lock(&block_mutex);
		if (res < XDAG_BLOCK_FIELDS && mining && pretop && pretop->time < send_time) {
			log_block("Mintop", pretop->hash, pretop->time, pretop->storage_pos);
			setfld(XDAG_FIELD_OUT, pretop->hash, xdag_hashlow_t);
			res++;
		}

//		for (oref = g_orphan_first[0]; oref && res < XDAG_BLOCK_FIELDS; oref = oref->next) {
//			ref = oref->orphan_bi;
//			if (ref->time < send_time) {
//				setfld(XDAG_FIELD_OUT, ref->hash, xdag_hashlow_t);
//				res++;
//			}
//		}
        struct xdag_block xb;
		struct block_internal bi;
        xdag_hash_t xb_hash = {0};
        int bi_retcode = 1;

		if(!xdag_rsdb_seek_orpblock(&xb)){
            xdag_hash(&xb, sizeof(struct xdag_block), xb_hash);
            bi_retcode = xdag_rsdb_get_bi(xb_hash, &bi);
		}

        for (;!bi_retcode && res < XDAG_BLOCK_FIELDS;) {
            if (bi.time < send_time) {
                setfld(XDAG_FIELD_OUT, bi.hash, xdag_hashlow_t);
                res++;
            }
            remove_orphan(bi.hash);
            if(!xdag_rsdb_seek_orpblock(&xb)) {
                xdag_hash(&xb, sizeof(struct xdag_block), xb_hash);
                xdag_rsdb_get_bi(xb_hash, &bi);
            }
        }
		pthread_mutex_unlock(&block_mutex);
	}

	for (j = 0; j < inputsCount; ++j) {
		setfld(XDAG_FIELD_IN, fields + j, xdag_hash_t);
	}

	for (j = 0; j < outputsCount; ++j) {
		setfld(XDAG_FIELD_OUT, fields + inputsCount + j, xdag_hash_t);
	}

	if(hasRemark) {
		setfld(XDAG_FIELD_REMARK, fields + inputsCount + outputsCount, xdag_remark_t);
	}

	if(mining && has_pool_tag) {
		setfld(XDAG_FIELD_REMARK, g_pool_tag, xdag_remark_t);
	}

	for (j = 0; j < nkeysnum; ++j) {
		key = keys + keysnum[j];
		block[0].field[0].type |= (uint64_t)((j == outsigkeyind ? XDAG_FIELD_SIGN_OUT : XDAG_FIELD_SIGN_IN) * 0x11) << ((i + j + nkeysnum) * 4);
		setfld(XDAG_FIELD_PUBLIC_KEY_0 + ((uintptr_t)key->pub & 1), (uintptr_t)key->pub & ~1l, xdag_hash_t);
	}

	if(outsigkeyind < 0) {
		block[0].field[0].type |= (uint64_t)(XDAG_FIELD_SIGN_OUT * 0x11) << ((i + j + nkeysnum) * 4);
	}

	for (j = 0; j < nkeysnum; ++j, i += 2) {
		key = keys + keysnum[j];
		hash_for_signature(block, key, signatureHash);
		xdag_sign(key->key, signatureHash, block[0].field[i].data, block[0].field[i + 1].data);
	}

	if (outsigkeyind < 0) {
		hash_for_signature(block, defkey, signatureHash);
		xdag_sign(defkey->key, signatureHash, block[0].field[i].data, block[0].field[i + 1].data);
	}

	if (mining) {
		if(!do_mining(block, &pretop, send_time)) {
			goto begin;
		}
	}

	xdag_hash(block, sizeof(struct xdag_block), newBlockHash);

	if(mining) {
		memcpy(g_xdag_mined_hashes[MAIN_TIME(send_time) & (CONFIRMATIONS_COUNT - 1)],
			newBlockHash, sizeof(xdag_hash_t));
		memcpy(g_xdag_mined_nonce[MAIN_TIME(send_time) & (CONFIRMATIONS_COUNT - 1)],
			block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(xdag_hash_t));
	}

	//log_block("Create", newBlockHash, block[0].field[0].time, 1);
	
	if(block_hash_result != NULL) {
		memcpy(block_hash_result, newBlockHash, sizeof(xdag_hash_t));
	}

	struct xdag_block *new_block = (struct xdag_block *)malloc(sizeof(struct xdag_block));
	if(new_block) {
		memcpy(new_block, block, sizeof(struct xdag_block));
	}	
	return new_block;
}

/* create and publish a block
* The first 'ninput' field 'fields' contains the addresses of the inputs and the corresponding quantity of XDAG,
* in the following 'noutput' fields similarly - outputs, fee; send_time (time of sending the block);
* if it is greater than the current one, then the mining is performed to generate the most optimal hash
*/
int xdag_create_and_send_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark,
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result)
{
	struct xdag_block *block = xdag_create_block(fields, inputsCount, outputsCount, hasRemark, fee, send_time, block_hash_result);
	if(!block) {
		return 0;
	}

	block->field[0].transport_header = 1;
	int res = xdag_add_block(block);
	if(res > 0) {
		xdag_send_new_block(block);
		res = 1;
	} else {
		res = 0;
	}
	free(block);

	return res;
}

int do_mining(struct xdag_block *block, struct block_internal **pretop, xtime_t send_time)
{
	uint64_t taskIndex = g_xdag_pool_task_index + 1;
	struct xdag_pool_task *task = &g_xdag_pool_task[taskIndex & 1];

	GetRandBytes(block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(xdag_hash_t));

	task->task_time = MAIN_TIME(send_time);

	xdag_hash_init(task->ctx0);
	xdag_hash_update(task->ctx0, block, sizeof(struct xdag_block) - 2 * sizeof(struct xdag_field));
	xdag_hash_get_state(task->ctx0, task->task[0].data);
	xdag_hash_update(task->ctx0, block[0].field[XDAG_BLOCK_FIELDS - 2].data, sizeof(struct xdag_field));
	memcpy(task->ctx, task->ctx0, xdag_hash_ctx_size());

	xdag_hash_update(task->ctx, block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field) - sizeof(uint64_t));
	memcpy(task->task[1].data, block[0].field[XDAG_BLOCK_FIELDS - 2].data, sizeof(struct xdag_field));
	memcpy(task->nonce.data, block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field));
	memcpy(task->lastfield.data, block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field));

	xdag_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);
	g_xdag_pool_task_index = taskIndex;

	while(xdag_get_xtimestamp() <= send_time) {
		sleep(1);
		pthread_mutex_lock(&g_create_block_mutex);
        struct block_internal *pretop_new = pretop_block();


		pthread_mutex_unlock(&g_create_block_mutex);
		if(*pretop != pretop_new && xdag_get_xtimestamp() < send_time) {
			*pretop = pretop_new;
			xdag_info("Mining: start from beginning because of pre-top block changed");
			return 0;
		}
	}

	pthread_mutex_lock((pthread_mutex_t*)g_ptr_share_mutex);
	memcpy(block[0].field[XDAG_BLOCK_FIELDS - 1].data, task->lastfield.data, sizeof(struct xdag_field));
	pthread_mutex_unlock((pthread_mutex_t*)g_ptr_share_mutex);

	return 1;
}

//static void reset_callback(struct ldus_rbtree *node)
//{
//	struct block_internal *bi = 0;
//
//	if(g_bi_index_enable) {
//		struct block_internal_index *index = (struct block_internal_index *)node;
//		bi = index->bi;
//	} else {
//		bi = (struct block_internal *)node;
//	}
//
//	struct block_backrefs *tmp;
//	for(struct block_backrefs *to_free = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); to_free != NULL;){
//		tmp = to_free->next;
//		xdag_free(to_free);
//		to_free = tmp;
//	}
//	if((bi->flags & BI_REMARK) && bi->remark != (uintptr_t)NULL) {
//		xdag_free((char*)bi->remark);
//	}
//	xdag_free(bi);
//
//	if(g_bi_index_enable) {
//		free(node);
//	}
//}

// main thread which works with block
static void *work_thread(void *arg)
{
	xtime_t t = XDAG_ERA, conn_time = 0, sync_time = 0, t0;
	int n_mining_threads = (int)(unsigned)(uintptr_t)arg, sync_thread_running = 0;
	uint64_t nhashes0 = 0, nhashes = 0;
	pthread_t th;
	uint64_t last_nmain = 0, nmain;
	time_t last_time_nmain_unequal = time(NULL);

begin:
	// loading block from the local storage
	g_xdag_state = XDAG_STATE_LOAD;
	//xdag_mess("Loading blocks from local storage...");

	xtime_t start = xdag_get_xtimestamp();
	xdag_show_state(0);

    xdag_mess("Loading xdag rocksdb blocks from local storage...");

    if(xdag_rsdb_init() == XDAG_RSDB_INIT_NEW) {
        xdag_load_blocks(t, xdag_get_xtimestamp(), &t, &add_block_callback_sync);
    }

	xdag_mess("Finish loading blocks, time cost %ldms", xdag_get_xtimestamp() - start);

	// waiting for command "run"
	while (!g_xdag_run) {
		g_xdag_state = XDAG_STATE_STOP;
		sleep(1);
	}

	// launching of synchronization thread
	g_xdag_sync_on = 1;
	if (is_pool() && !sync_thread_running) {
		xdag_mess("Starting sync thread...");
        pthread_attr_t * attr_sync_thread = NULL;
#if defined(__APPLE__)
        pthread_attr_t attr_sync;
        struct rlimit lim;
        if (getrlimit(RLIMIT_STACK, &lim))
            abort();

        attr_sync_thread = &attr_sync;
        if (pthread_attr_init(attr_sync_thread)){
            printf("set sync thread stack size failed \n");
            abort();
        }

        if (pthread_attr_setstacksize(attr_sync_thread, lim.rlim_max)){
            printf("set sync thread stack size failed \n");
            abort();
        }
#endif
        int err = pthread_create(&th, attr_sync_thread, sync_thread, (void*)(uintptr_t)0);
		if(err != 0) {
			printf("create sync_thread failed, error : %s\n", strerror(err));
			return 0;
		}

		sync_thread_running = 1;

		err = pthread_detach(th);
		if(err != 0) {
			printf("detach sync_thread failed, error : %s\n", strerror(err));
			return 0;
		}
	}

	if (is_wallet()) {
		// start mining threads
		xdag_mess("Starting mining threads...");
		xdag_mining_start(n_mining_threads);
	}

	// periodic generation of blocks and determination of the main block
	xdag_mess("Entering main cycle...");

	for (;;) {
		unsigned nblk;

		t0 = t;
		t = xdag_get_xtimestamp();
		nhashes0 = nhashes;
		nhashes = g_xdag_extstats.nhashes;
		nmain = g_xdag_stats.nmain;

		if (t > t0) {
			g_xdag_extstats.hashrate_s = ((double)(nhashes - nhashes0) * 1024) / (t - t0);
		}

		if (!g_block_production_on && is_pool() &&
				(g_xdag_state == XDAG_STATE_WAIT || g_xdag_state == XDAG_STATE_WTST ||
				g_xdag_state == XDAG_STATE_SYNC || g_xdag_state == XDAG_STATE_STST || 
				g_xdag_state == XDAG_STATE_CONN || g_xdag_state == XDAG_STATE_CTST)) {
			if (g_xdag_state == XDAG_STATE_SYNC || g_xdag_state == XDAG_STATE_STST || 
					g_xdag_stats.nmain >= (MAIN_TIME(t) - xdag_get_start_frame())) {
				g_block_production_on = 1;
			} else if (last_nmain != nmain) {
				last_nmain = nmain;
				last_time_nmain_unequal = time(NULL);
			} else if (time(NULL) - last_time_nmain_unequal > MAX_TIME_NMAIN_STALLED) {
				g_block_production_on = 1;
			}
		}

		if (g_block_production_on && 
				(nblk = (unsigned)g_xdag_extstats.nnoref / (XDAG_BLOCK_FIELDS - 5))) {
			nblk = nblk / 61 + (nblk % 61 > (unsigned)rand() % 61);

			xdag_mess("Starting refer blocks creation...");
			while (nblk--) {
				xdag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
			}
		}

		if (g_xdag_state == XDAG_STATE_REST) {
			g_xdag_sync_on = 0;

			xdag_mining_start(0);

			while (xdag_get_xtimestamp() - t < MAIN_CHAIN_PERIOD + (3 << 10)) {
				sleep(1);
			}
			g_balance = 0;
			top_main_chain = pretop_main_chain = 0;
            memset(g_top_main_chain_hash, 0, sizeof(xdag_hash_t));
			memset(&g_xdag_stats, 0, sizeof(g_xdag_stats));
			memset(&g_xdag_extstats, 0, sizeof(g_xdag_extstats));
			conn_time = sync_time = 0;

			goto begin;
		} else {
			time_t last_received = atomic_load_explicit_uint_least64(&g_xdag_last_received, memory_order_relaxed);

			if (t > (last_received << 10) && t - (last_received << 10) > 3 * MAIN_CHAIN_PERIOD) {
				g_xdag_state = (is_wallet() ? (g_xdag_testnet ? XDAG_STATE_TTST : XDAG_STATE_TRYP)
					: (g_xdag_testnet ? XDAG_STATE_WTST : XDAG_STATE_WAIT));
				conn_time = sync_time = 0;
			} else {
				if (!conn_time) {
					conn_time = t;
				}

				if (is_pool() && t - conn_time >= 2 * MAIN_CHAIN_PERIOD
					&& !memcmp(&g_xdag_stats.difficulty, &g_xdag_stats.max_difficulty, sizeof(xdag_diff_t))) {
					sync_time = t;
				}

				if (t - (g_xdag_xfer_last << 10) <= 2 * MAIN_CHAIN_PERIOD + 4) {
					g_xdag_state = XDAG_STATE_XFER;
				} else if (is_wallet()) {
					g_xdag_state = (g_xdag_mining_threads > 0 ?
						(g_xdag_testnet ? XDAG_STATE_MTST : XDAG_STATE_MINE)
						: (g_xdag_testnet ? XDAG_STATE_PTST : XDAG_STATE_POOL));
				} else if (t - sync_time > 8 * MAIN_CHAIN_PERIOD) {
					g_xdag_state = (g_xdag_testnet ? XDAG_STATE_CTST : XDAG_STATE_CONN);
				} else {
					g_xdag_state = (g_xdag_testnet ? XDAG_STATE_STST : XDAG_STATE_SYNC);
				}
			}
		}

		if (is_pool()) {
			check_new_main();
		}

        struct block_internal ours;
		pthread_mutex_unlock(&block_mutex);
		xdag_show_state(!xdag_rsdb_get_ourbi(ourlast_hash, &ours) ? ours.hash : 0);

		while (xdag_get_xtimestamp() - t < 1024) {
			sleep(1);
		}
	}

	return 0;
}

/* start of regular block processing
 * n_mining_threads - the number of threads for mining on the CPU;
 *   for the light node is_pool == 0;
 * miner_address = 1 - the address of the miner is explicitly set
 */
int xdag_blocks_start(int mining_threads_count, int miner_address)
{
	pthread_mutexattr_t attr;
	pthread_t th;

//	if (xdag_mem_init(is_wallet() && !miner_address ? 0 : (((xdag_get_xtimestamp() - XDAG_ERA) >> 10) + (uint64_t)365 * 24 * 60 * 60) * 2 * sizeof(struct block_internal))) {
//		return -1;
//	}

	//g_bi_index_enable = g_use_tmpfile;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&block_mutex, &attr);
	pthread_mutex_init(&rbtree_mutex, 0);

	pthread_attr_t * attr_work_thread = NULL;
#if defined(__APPLE__)
	pthread_attr_t attr_storage;
	struct rlimit lim;
#endif

#if defined(__APPLE__)
	if (getrlimit(RLIMIT_STACK, &lim))
		abort();

	attr_work_thread = &attr_storage;
	if (pthread_attr_init(attr_work_thread)){
		printf("set work thread stack size failed \n");
		abort();
	}

	if (pthread_attr_setstacksize(attr_work_thread, lim.rlim_max)){
		printf("set work thread stack size failed \n");
		abort();
	}
#endif

	int err = pthread_create(&th, attr_work_thread, work_thread, (void*)(uintptr_t)(unsigned)mining_threads_count);
	if(err != 0) {
		printf("create work_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	err = pthread_detach(th);
	if(err != 0) {
		printf("create pool_main_thread failed, error : %s\n", strerror(err));
		return -1;
	}

#if defined(__APPLE__)
	if (attr_work_thread != NULL){
		pthread_attr_destroy(attr_work_thread);
	}
#endif

	return 0;
}

/* returns our first block. If there is no blocks yet - the first block is created. */
int xdag_get_our_block(xdag_hash_t hash)
{
	pthread_mutex_lock(&block_mutex);

    struct block_internal bi;
	pthread_mutex_unlock(&block_mutex);

	if (xdag_rsdb_get_ourbi(ourfirst_hash, &bi)) {
	    // TODO rethink address block
		xdag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
		pthread_mutex_lock(&block_mutex);
        int retcode = xdag_rsdb_get_ourbi(ourfirst_hash, &bi);
		pthread_mutex_unlock(&block_mutex);
		if (retcode) {
			return -1;
		}
	}
	memcpy(hash, bi.hash, sizeof(xdag_hash_t));
	return 0;
}

/* calls callback for each own block */
int xdag_traverse_our_blocks(void *data,
    int (*callback)(void*, xdag_hash_t, xdag_amount_t, xtime_t, int))
{
	int res = 0;

	pthread_mutex_lock(&block_mutex);

    struct block_internal bi;
    int retcode = xdag_rsdb_get_ourbi(ourfirst_hash, &bi);

	for (;!res && !retcode; ) {
		res = (*callback)(data, bi.hash, bi.amount, bi.time, bi.n_our_key);
        xdag_rsdb_get_ourbi(bi.ournext, &bi);
	}
	pthread_mutex_unlock(&block_mutex);
	return res;
}

static int (*g_traverse_callback)(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time);
static void *g_traverse_data;

static void traverse_all_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = NULL;
//	if(g_bi_index_enable) {
//		struct block_internal_index *index = (struct block_internal_index *)node;
//		bi = index->bi;
//	} else {
		bi = (struct block_internal *)node;
//	}

	(*g_traverse_callback)(g_traverse_data, bi->hash, bi->amount, bi->time);
}

/* calls callback for each block */
int xdag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
	xdag_amount_t amount, xtime_t time))
{
	pthread_mutex_lock(&block_mutex);
    rocksdb_iterator_t* iter = NULL;
    iter = rocksdb_create_iterator(g_xdag_rsdb->db, g_xdag_rsdb->read_options);
    char key[1] = {[0] = HASH_BLOCK_INTERNAL};
    size_t klen = 1;
    char address[33] = {0};
    for (rocksdb_iter_seek(iter, key, 1);
         rocksdb_iter_valid(iter) && !memcmp(rocksdb_iter_key(iter, &klen), key, 1);
         rocksdb_iter_next(iter))
    {
        size_t vlen = 0;
        struct block_internal* bi = (struct block_internal*)rocksdb_iter_value(iter, &vlen);
        if(bi) {
            xdag_hash2address(bi->hash, address);
            printf("%s  %20.9Lf\n", address, amount2xdags(bi->amount));
        }
    }
	pthread_mutex_unlock(&block_mutex);
	return 0;
}

/* returns current balance for specified address or balance for all addresses if hash == 0 */
xdag_amount_t xdag_get_balance(xdag_hash_t hash)
{
    struct block_internal bi;
    xdag_amount_t amount = 0;
	if (!hash) {
		return g_balance;
	}

    if(xdag_rsdb_get_bi(hash, &bi)) {
        return 0;
    }
    amount = bi.amount;
	return amount;
}

/* sets current balance for the specified address */
int xdag_set_balance(xdag_hash_t hash, xdag_amount_t balance)
{
	if (!hash) return -1;
    struct block_internal bi, ourfirst_bi, *ourlast_bi= NULL;
	pthread_mutex_lock(&block_mutex);
    if(!xdag_rsdb_get_bi(hash, &bi)) {
        return -1;
    }

    if(!xdag_rsdb_get_bi(hash, &ourfirst_bi)) {
        return -1;
    }

//    if (bi->flags & BI_OURS && bi != ourfirst) {
//        if (bi->ourprev) {
//            bi->ourprev->ournext = bi->ournext;
//        } else {
//            ourfirst = bi->ournext;
//        }
//        if (bi->ournext) {
//            bi->ournext->ourprev = bi->ourprev;
//        } else {
//            ourlast = bi->ourprev;
//        }
//        bi->ourprev = 0;
//        bi->ournext = ourfirst;
//        if (ourfirst) {
//            ourfirst->ourprev = bi;
//        } else {
//            ourlast = bi;
//        }
//        ourfirst = bi;
//    }

	if (bi.flags & BI_OURS && memcmp(bi.hash, ourfirst_bi.hash, sizeof(xdag_hashlow_t))) {
        struct block_internal ourprev_bi;
        struct block_internal ournext_bi;
        ;
//        if (bi->ourprev) {
//            bi->ourprev->ournext = bi->ournext;
//        } else {
//            ourfirst = bi->ournext;
//        }
		if (!xdag_rsdb_get_ourbi(bi.ourprev, &ourprev_bi)) {
            memcpy(ourprev_bi.ournext, bi.ournext, sizeof(xdag_hashlow_t));
            xdag_rsdb_put_ourbi(&ourprev_bi);
		} else {
            memcpy(ourfirst_hash, bi.ournext, sizeof(xdag_hash_t));
            xdag_rsdb_put_setting(SETTING_OUR_FIRST_HASH, (const char*)ourfirst_hash, sizeof(ourfirst_hash));
		}

//        if (bi->ournext) {
//            bi->ournext->ourprev = bi->ourprev;
//        } else {
//            ourlast = bi->ourprev;
//        }

		if(!xdag_rsdb_get_ourbi(bi.ournext, &ournext_bi)) {
            memcpy(ournext_bi.ourprev, bi.ourprev, sizeof(xdag_hashlow_t));
            xdag_rsdb_put_ourbi(&ournext_bi);
		} else {
            memcpy(ourlast_hash, ourprev_bi.hash, sizeof(xdag_hash_t));
            xdag_rsdb_put_setting(SETTING_OUR_LAST_HASH, (const char*)ourlast_hash, sizeof(ourlast_hash));
		}

//        bi->ourprev = 0;
//        bi->ournext = ourfirst;
        memset(bi.ourprev, 0, sizeof(xdag_hashlow_t));
        memcpy(bi.ournext, ourfirst_hash, sizeof(xdag_hashlow_t));

//        if (ourfirst) {
//            ourfirst->ourprev = bi;
//        } else {
//            ourlast = bi;
//        }

        struct block_internal ourfirst;
		if (!xdag_rsdb_get_ourbi(ourfirst_hash, &ourfirst)) {
            memcpy(ourfirst.ourprev, bi.hash, sizeof(xdag_hashlow_t));
		} else {
            memcpy(ourlast_hash, bi.hash, sizeof(xdag_hash_t));
            xdag_rsdb_put_setting(SETTING_OUR_LAST_HASH, (const char*)ourlast_hash, sizeof(ourlast_hash));
		}

//		ourfirst = bi;
        memcpy(ourfirst_hash, bi.hash, sizeof(xdag_hash_t));
        xdag_rsdb_put_setting(SETTING_OUR_FIRST_HASH, (const char*)ourfirst_hash, sizeof(ourfirst_hash));
	}
	pthread_mutex_unlock(&block_mutex);

//    if (bi->amount != balance) {
//        xdag_hash_t hash0;
//        xdag_amount_t diff;
//        memset(hash0, 0, sizeof(xdag_hash_t));
//
//        if (balance > bi->amount) {
//            diff = balance - bi->amount;
//            xdag_log_xfer(hash0, hash, diff);
//            if (bi->flags & BI_OURS) {
//                g_balance += diff;
//            }
//        } else {
//            diff = bi->amount - balance;
//            xdag_log_xfer(hash, hash0, diff);
//            if (bi->flags & BI_OURS) {
//                g_balance -= diff;
//            }
//        }
//
//        bi->amount = balance;
//    }

	if (bi.amount != balance) {
        xdag_hash_t hash0 = {0};
        xdag_amount_t diff = 0;

		if (balance > bi.amount) {
			diff = balance - bi.amount;
			xdag_log_xfer(hash0, hash, diff);
			if (bi.flags & BI_OURS) {
				g_balance += diff;
			}
		} else {
			diff = bi.amount - balance;
			xdag_log_xfer(hash, hash0, diff);
			if (bi.flags & BI_OURS) {
				g_balance -= diff;
			}
		}
		bi.amount = balance;
        xdag_rsdb_put_ourbi(&bi);
	}
	return 0;
}

// returns position and time of block by hash; if block is extra and block != 0 also returns the whole block
int64_t xdag_get_block_pos(xdag_hash_t hash, xtime_t *t, struct xdag_block *block)
{
    struct block_internal bi;
    int64_t pos = 0;

	if (xdag_rsdb_get_bi(hash, &bi)) {
		return -1;
	}

	if (block && bi.flags & BI_EXTRA) {
        struct xdag_block xb;
        if(!xdag_rsdb_get_orpblock(hash, &xb)) {
            memcpy(block, &xb, sizeof(struct xdag_block));
        }
	}

	if (block) pthread_mutex_unlock(&block_mutex);

	*t = bi.time;
    pos = bi.storage_pos;
	return pos;
}

//returns a number of key by hash of block, or -1 if block is not ours
int xdag_get_key(xdag_hash_t hash)
{
    struct block_internal bi;
    int n_our_key = 0;
	if (xdag_rsdb_get_bi(hash, &bi) || !(bi.flags & BI_OURS)) {
		return -1;
	}
    n_our_key = bi.n_our_key;
	return n_our_key;
}

/* reinitialization of block processing */
int xdag_blocks_reset(void)
{
	pthread_mutex_lock(&block_mutex);
    if (g_xdag_state != XDAG_STATE_REST) {
        xdag_crit("The local storage is corrupted. Resetting blocks engine.");
        g_xdag_state = XDAG_STATE_REST;
        xdag_show_state(0);
    }
    pthread_mutex_unlock(&block_mutex);

	return 0;
}

#define pramount(amount) xdag_amount2xdag(amount), xdag_amount2cheato(amount)

// returns string representation for the block state. Ignores BI_OURS flag
const char* xdag_get_block_state_info(uint8_t flags)
{
	const uint8_t flag = flags & ~(BI_OURS | BI_REMARK);

	if(flag == (BI_REF | BI_MAIN_REF | BI_APPLIED | BI_MAIN | BI_MAIN_CHAIN)) { //1F
		return "Main";
	}
	if(flag == (BI_REF | BI_MAIN_REF | BI_APPLIED)) { //1C
		return "Accepted";
	}
	if(flag == (BI_REF | BI_MAIN_REF)) { //18
		return "Rejected";
	}
	return "Pending";
}

void append_block_info(struct block_internal *bi)
{
#ifndef _WIN32
	// if websocket service is not running return directly
	if(!g_websocket_running) {
		return;
	}
    
    int flags, nlinks;
    xdag_hashlow_t ref, link[MAX_LINKS];
    pthread_mutex_lock(&block_mutex);
    //ref = bi->ref;
    memcpy(ref, bi->ref, sizeof(ref));
    flags = bi->flags;
    nlinks = bi->nlinks;
    memcpy(link, bi->link, nlinks * sizeof(struct block_internal*));
    pthread_mutex_unlock(&block_mutex);

	char time_buf[64] = {0};
	char address[33] = {0};
	uint64_t *h = bi->hash;
	xdag_hash2address(h, address);
	xdag_xtime_to_string(bi->time, time_buf);

	char message[4096] = {0};
	char buf[128] = {0};

	sprintf(message,
			"{\"time\":\"%s\""
			",\"flags\":\"%x\""
			",\"state\":\"%s\""
			",\"hash\":\"%016llx%016llx%016llx%016llx\""
			",\"difficulty\":\"%llx%016llx\""
			",\"remark\":\"%s\""
			",\"address\":\"%s\""
			",\"balance\":\"%u.%09u\""
			",\"fields\":["
			, time_buf
			, flags & ~BI_OURS
			, xdag_get_block_state_info(flags)
			, (unsigned long long)h[3], (unsigned long long)h[2], (unsigned long long)h[1], (unsigned long long)h[0]
			, xdag_diff_args(bi->difficulty)
			, get_remark(bi)
			, address
			, pramount(bi->amount)
			);

	if((flags & BI_REF)) {
		xdag_hash2address(ref, address);
	} else {
		strcpy(address, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
	}
	sprintf(buf, "{\"direction\":\"fee\",\"address\":\"%s\",\"amount\":\"%u.%09u\"}", address, pramount(bi->fee));
	strcat(message, buf);

	for (int i = 0; i < nlinks; ++i) {
		xdag_hash2address(link[i], address);
		sprintf(buf, ",{\"direction\":\"%s\",\"address\":\"%s\",\"amount\":\"%u.%09u\"}",  (1 << i & bi->in_mask ? " input" : "output"),address, pramount(bi->linkamount[i]));
		strcat(message, buf);
	}

	strcat(message, "]}");

	xdag_ws_message_append(message);
#endif
}

/* prints detailed information about block */
int xdag_print_block_info(xdag_hash_t hash, FILE *out)
{
	char time_buf[64] = {0};
	char address[33] = {0};
    xdag_amount_t amount = 0;
	int i;

    struct block_internal tbi, *bi = NULL;
	if (xdag_rsdb_get_bi(hash, &tbi)) {
		return -1;
	}
    bi = &tbi;
	uint64_t *h = bi->hash;
	xdag_xtime_to_string(bi->time, time_buf);
	fprintf(out, "      time: %s\n", time_buf);
	fprintf(out, " timestamp: %llx\n", (unsigned long long)bi->time);
	fprintf(out, "     flags: %x\n", bi->flags & ~BI_OURS);
	fprintf(out, "     state: %s\n", xdag_get_block_state_info(bi->flags));
	fprintf(out, "  file pos: %llx\n", (unsigned long long)bi->storage_pos);
    fprintf(out, "      file: storage%s/%02x/%02x/%02x/%02x.dat\n", (g_xdag_testnet ? "-testnet" : ""),
        (int)((bi->time) >> 40)&0xff, (int)((bi->time) >> 32)&0xff, (int)((bi->time) >> 24)&0xff, (int)((bi->time) >> 16)&0xff);
	fprintf(out, "      hash: %016llx%016llx%016llx%016llx\n",
		(unsigned long long)h[3], (unsigned long long)h[2], (unsigned long long)h[1], (unsigned long long)h[0]);
	fprintf(out, "    remark: %s\n", get_remark(bi));
	fprintf(out, "difficulty: %llx%016llx\n", xdag_diff_args(bi->difficulty));
	xdag_hash2address(h, address);
	fprintf(out, "   balance: %s  %10u.%09u\n", address, pramount(bi->amount));
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "                               block as transaction: details\n");
	fprintf(out, " direction  address                                    amount\n");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	int flags;
    xdag_hash_t ref = {0};
	pthread_mutex_lock(&block_mutex);
	//ref = bi->ref;
    memcpy(ref, bi->ref, sizeof(ref));
	flags = bi->flags;
	pthread_mutex_unlock(&block_mutex);
	if((flags & BI_REF)) {
		xdag_hash2address(ref, address);
	} else {
		strcpy(address, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
	}
	fprintf(out, "       fee: %s  %10u.%09u\n", address, pramount(bi->fee));

 	if(flags & BI_EXTRA) pthread_mutex_lock(&block_mutex);
 	int nlinks = bi->nlinks;
	xdag_hashlow_t link[MAX_LINKS];
	memcpy(link, bi->link, nlinks * sizeof(xdag_hashlow_t));
	if(flags & BI_EXTRA) pthread_mutex_unlock(&block_mutex);

 	for (i = 0; i < nlinks; ++i) {
		xdag_hash2address(link[i], address);
		fprintf(out, "    %6s: %s  %10u.%09u\n", (1 << i & bi->in_mask ? " input" : "output"),
			address, pramount(bi->linkamount[i]));
	}

//	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
//	fprintf(out, "                                 block as address: details\n");
//	fprintf(out, " direction  transaction                                amount       time                     remark                          \n");
//	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");

//	int N = 0x10000;
//	int n = 0;
//	struct block_internal **ba = malloc(N * sizeof(struct block_internal *));
//
//	if (!ba) return -1;
//
//	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
//		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);
//
//		if (!i) {
//			continue;
//		}
//
//		if (n + i > N) {
//			N *= 2;
//			struct block_internal **ba1 = realloc(ba, N * sizeof(struct block_internal *));
//			if (!ba1) {
//				free(ba);
//				return -1;
//			}
//
//			ba = ba1;
//		}
//
//		memcpy(ba + n, br->backrefs, i * sizeof(struct block_internal *));
//		n += i;
//	}
//
//	if (n) {
//		qsort(ba, n, sizeof(struct block_internal *), bi_compar);
//
//		for (i = 0; i < n; ++i) {
//			if (!i || ba[i] != ba[i - 1]) {
//				struct block_internal *ri = ba[i];
//				if (ri->flags & BI_APPLIED) {
//					for (int j = 0; j < ri->nlinks; j++) {
//						if(ri->link[j] == bi && ri->linkamount[j]) {
//							xdag_xtime_to_string(ri->time, time_buf);
//							xdag_hash2address(ri->hash, address);
//							fprintf(out, "    %6s: %s  %10u.%09u  %s  %s\n",
//								(1 << j & ri->in_mask ? "output" : " input"), address,
//								pramount(ri->linkamount[j]), time_buf, get_remark(ri));
//						}
//					}
//				}
//			}
//		}
//	}
//
//	free(ba);
//
//	if (bi->flags & BI_MAIN) {
//		xdag_hash2address(h, address);
//        if(MAIN_TIME(bi->time) - MAIN_TIME(XDAG_ERA) < MAIN_APOLLO_HIGHT) {
//            amount = MAIN_START_AMOUNT;
//        } else {
//            amount = MAIN_APOLLO_AMOUNT;
//        }
//		fprintf(out, "   earning: %s  %10u.%09u  %s\n", address,
//			pramount(amount >> ((MAIN_TIME(bi->time) - MAIN_TIME(XDAG_ERA)) >> MAIN_BIG_PERIOD_LOG)),
//			time_buf);
//	}
	
	return 0;
}

static inline void print_block(struct block_internal *block, int print_only_addresses, FILE *out)
{
	char address[33] = {0};
	char time_buf[64] = {0};

	xdag_hash2address(block->hash, address);

	if(print_only_addresses) {
		fprintf(out, "%s\n", address);
	} else {
		xdag_xtime_to_string(block->time, time_buf);
		fprintf(out, "%s   %s   %-8s  %-32s\n", address, time_buf, xdag_get_block_state_info(block->flags), get_remark(block));
	}
}

static inline void print_header_block_list(FILE *out)
{
	fprintf(out, "---------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "address                            time                      state     mined by                          \n");
	fprintf(out, "---------------------------------------------------------------------------------------------------------\n");
}

// prints list of N last main blocks
void xdag_list_main_blocks(int count, int print_only_addresses, FILE *out)
{
	int i = 0;
	if(!print_only_addresses) {
		print_header_block_list(out);
	}
    struct block_internal b;
	int retcode = xdag_rsdb_get_bi(g_top_main_chain_hash, &b);
    for (; !retcode && i < count; ) {
        if (b.flags & BI_MAIN) {
            print_block(&b, print_only_addresses, out);
            ++i;
        }
        retcode = xdag_rsdb_get_bi(b.link[b.max_diff_link], &b);
    }
}

// prints list of N last blocks mined by current pool
// TODO: find a way to find non-payed mined blocks or remove 'include_non_payed' parameter
void xdag_list_mined_blocks(int count, int include_non_payed, FILE *out)
{
	int i = 0;
	print_header_block_list(out);
    struct block_internal b;
    int retcode = xdag_rsdb_get_bi(g_top_main_chain_hash, &b);
    for(; !retcode && i < count;) {
        if(b.flags & BI_MAIN && b.flags & BI_OURS) {
            print_block(&b, 0, out);
            ++i;
        }
        retcode = xdag_rsdb_get_bi(b.link[b.max_diff_link], &b);
    }
}

//void cache_retarget(int32_t cache_hit, int32_t cache_miss)
//{
//	if(g_xdag_extstats.cache_usage >= g_xdag_extstats.cache_size) {
//		if(g_xdag_extstats.cache_hitrate < 0.94 && g_xdag_extstats.cache_size < CACHE_MAX_SIZE) {
//			g_xdag_extstats.cache_size++;
//		} else if(g_xdag_extstats.cache_hitrate > 0.98 && !cache_miss && g_xdag_extstats.cache_size && (rand() & 0xF) < 0x5) {
//			g_xdag_extstats.cache_size--;
//		}
//		for(uint32_t l = g_xdag_extstats.cache_usage; l > g_xdag_extstats.cache_size; l--) {
//			if(cache_first != NULL) {
//				struct cache_block* to_free = cache_first;
//				cache_first = cache_first->next;
//				if(cache_first == NULL) {
//					cache_last = NULL;
//				}
//				ldus_rbtree_remove(&cache_root, &to_free->node);
//				free(to_free);
//				g_xdag_extstats.cache_usage--;
//			} else {
//				break;
//				xdag_warn("Non critical error, break in for [function: cache_retarget]");
//			}
//		}
//
//	} else if(g_xdag_extstats.cache_hitrate > 0.98 && !cache_miss && g_xdag_extstats.cache_size && (rand() & 0xF) < 0x5) {
//		g_xdag_extstats.cache_size--;
//	}
//	if((uint32_t)(g_xdag_extstats.cache_size / 0.9) > CACHE_MAX_SIZE) {
//		g_xdag_extstats.cache_size = (uint32_t)(g_xdag_extstats.cache_size*0.9);
//	}
//	if(cache_hit + cache_miss > 0) {
//		if(cache_bounded_counter < CACHE_MAX_SAMPLES)
//			cache_bounded_counter++;
//		g_xdag_extstats.cache_hitrate = moving_average_double(g_xdag_extstats.cache_hitrate, (double)((cache_hit) / (cache_hit + cache_miss)), cache_bounded_counter);
//
//	}
//}

//void cache_add(struct xdag_block* block, xdag_hash_t hash)
//{
//	if(g_xdag_extstats.cache_usage <= CACHE_MAX_SIZE) {
//		struct cache_block *cacheBlock = malloc(sizeof(struct cache_block));
//		if(cacheBlock != NULL) {
//			memset(cacheBlock, 0, sizeof(struct cache_block));
//			memcpy(&(cacheBlock->block), block, sizeof(struct xdag_block));
//			memcpy(&(cacheBlock->hash), hash, sizeof(xdag_hash_t));
//
//			if(cache_first == NULL)
//				cache_first = cacheBlock;
//			if(cache_last != NULL)
//				cache_last->next = cacheBlock;
//			cache_last = cacheBlock;
//			ldus_rbtree_insert(&cache_root, &cacheBlock->node);
//			g_xdag_extstats.cache_usage++;
//		} else {
//			xdag_warn("cache malloc failed [function: cache_add]");
//		}
//	} else {
//		xdag_warn("maximum cache reached [function: cache_add]");
//	}
//
//}

//int32_t check_signature_out_cached(struct block_internal* blockRef, struct xdag_public_key *public_keys, const int keysCount, int32_t *cache_hit, int32_t *cache_miss)
//{
//	struct cache_block *bref = cache_block_by_hash(blockRef->hash);
//	if(bref != NULL) {
//		++(*cache_hit);
//		return  find_and_verify_signature_out(&(bref->block), public_keys, keysCount);
//	} else {
//		++(*cache_miss);
//		return check_signature_out(blockRef, public_keys, keysCount);
//	}
//}

int32_t check_signature_out(struct block_internal *bi, struct xdag_public_key *public_keys, const int keysCount)
{
	struct xdag_block buf;
	struct xdag_block *bref = xdag_storage_load(bi->hash, bi->time, bi->storage_pos, &buf);
	if(!bref) {
		return 8;
	}
	return find_and_verify_signature_out(bref, public_keys, keysCount);
}

static int32_t find_and_verify_signature_out(struct xdag_block* bref, struct xdag_public_key *public_keys, const int keysCount)
{
	int j = 0;
	for(int k = 0; j < XDAG_BLOCK_FIELDS; ++j) {
		if(xdag_type(bref, j) == XDAG_FIELD_SIGN_OUT && (++k & 1)
			&& valid_signature(bref, j, keysCount, public_keys) >= 0) {
			break;
		}
	}
	if(j == XDAG_BLOCK_FIELDS) {
		return 9;
	}
	return 0;
}

int xdag_get_transactions(xdag_hash_t hash, void *data, int (*callback)(void*, int, int, xdag_hash_t, xdag_amount_t, xtime_t, const char *))
{
    struct block_internal bi;
    if (xdag_rsdb_get_bi(hash, &bi)) {
		return -1;
	}
	
	int size = 0x10000;
	int n = 0;
	struct block_internal **block_array = malloc(size * sizeof(struct block_internal *));
	
	if (!block_array) return -1;

//	int i;
//	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
//		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);
//
//		if (!i) {
//			continue;
//		}
//
//		if (n + i > size) {
//			size *= 2;
//			struct block_internal **tmp_array = realloc(block_array, size * sizeof(struct block_internal *));
//			if (!tmp_array) {
//				free(block_array);
//				return -1;
//			}
//
//			block_array = tmp_array;
//		}
//
//		memcpy(block_array + n, br->backrefs, i * sizeof(struct block_internal *));
//		n += i;
//	}
//
//	if (!n) {
//		free(block_array);
//		return 0;
//	}
//
//	qsort(block_array, n, sizeof(struct block_internal *), bi_compar);
//
//	for (i = 0; i < n; ++i) {
//		if (!i || block_array[i] != block_array[i - 1]) {
//			struct block_internal *ri = block_array[i];
//			for (int j = 0; j < ri->nlinks; j++) {
//				if(ri->link[j] == bi && ri->linkamount[j]) {
//					if(callback(data, 1 << j & ri->in_mask, ri->flags, ri->hash, ri->linkamount[j], ri->time, get_remark(ri))) {
//						free(block_array);
//						return n;
//					}
//				}
//			}
//		}
//	}
//
//	free(block_array);
	return n;
}

int remove_orphan(xdag_hashlow_t hash)
{
    struct block_internal b;
    if(!xdag_rsdb_get_bi(hash, &b)) {
        if(!(b.flags & BI_REF) ) {
            b.flags |= BI_REF;
            struct xdag_block xb;
            if(!xdag_rsdb_get_orpblock(hash, &xb)) {
                if((&b)->flags & BI_EXTRA) {
                    b.storage_pos = xdag_storage_save(&xb);
                    for (int i = 0; i < b.nlinks; ++i) {
                        remove_orphan(b.link[i]);
                    }
                    b.flags &= ~BI_EXTRA;
                    g_xdag_extstats.nextra--;
                } else {
                    g_xdag_extstats.nnoref--;
                }
            }
            xdag_rsdb_put_bi(&b);
        }
    }
    return 0;
}

void add_orphan(struct block_internal* bi, struct xdag_block* xb)
{
    if (bi && (bi->flags & BI_EXTRA)) {
        g_xdag_extstats.nextra++;
    } else {
        xdag_rsdb_put_orpblock(bi->hash, xb);
        g_xdag_extstats.nnoref++;
    }
}

void xdag_list_orphan_blocks(int count, FILE *out)
{
	int i = 0;
	print_header_block_list(out);

	pthread_mutex_lock(&block_mutex);

//	for(struct orphan_block *b = g_orphan_first[0]; b && i < count; b = b->next, i++) {
//		print_block(b->orphan_bi, 0, out);
//	}

	pthread_mutex_unlock(&block_mutex);
}

// completes work with the blocks
void xdag_block_finish()
{
	pthread_mutex_lock(&g_create_block_mutex);
	pthread_mutex_lock(&block_mutex);
}

int xdag_get_block_info(xdag_hash_t hash, void *info, int (*info_callback)(void*, int, xdag_hash_t, xdag_amount_t, xtime_t, uint64_t, const char *),
						void *links, int (*links_callback)(void*, const char *, xdag_hash_t, xdag_amount_t))
{
	pthread_mutex_lock(&block_mutex);
    struct block_internal bi;
    int retcode = xdag_rsdb_get_bi(hash, &bi);
	pthread_mutex_unlock(&block_mutex);

	if(info_callback && !retcode) {
		info_callback(info, bi.flags & ~BI_OURS,  bi.hash, bi.amount, bi.time, bi.storage_pos, get_remark(&bi));
	}

	if(links_callback && !retcode) {
		int flags;
		xdag_hash_t ref_hash = {0};
		pthread_mutex_lock(&block_mutex);
		//ref = bi->ref;
		memcpy(&ref_hash, bi.ref, sizeof(bi.ref));

		flags = bi.flags;
		pthread_mutex_unlock(&block_mutex);

		xdag_hash_t link_hash = {0};
		if((flags & BI_REF)) {
			memcpy(link_hash, ref_hash, sizeof(xdag_hash_t));
		}
		links_callback(links, "fee", link_hash, bi.fee);

		struct block_internal *bi_links[MAX_LINKS] = {0};
		int bi_nlinks = 0;

		if(flags & BI_EXTRA) {
			pthread_mutex_lock(&block_mutex);
		}

		bi_nlinks = bi.nlinks;
		memcpy(bi_links, bi.link, bi_nlinks * sizeof(struct block_internal *));

		if(flags & BI_EXTRA) {
			pthread_mutex_unlock(&block_mutex);
		}

		for (int i = 0; i < bi_nlinks; ++i) {
			links_callback(links, (1 << i & bi.in_mask ? "input" : "output"), bi_links[i]->hash, bi.linkamount[i]);
		}
	}
	return 0;
}

static inline size_t remark_acceptance(xdag_remark_t origin)
{
	size_t size = validate_remark((const char*)origin);
	if(size){
		return size;
	}
	return 0;
}

static int add_remark_bi(struct block_internal* bi, xdag_remark_t strbuf)
{
	size_t size = remark_acceptance(strbuf);
	memcpy(&bi->remark, strbuf, size);
	return 1;
}

//static void add_backref(struct block_internal* blockRef, struct block_internal* nodeBlock)
//{
//	int i = 0;
//
//	struct block_backrefs *tmp = (struct block_backrefs*)atomic_load_explicit_uintptr(&blockRef->backrefs, memory_order_acquire);
//	// LIFO list: if the first element doesn't exist or it is full, a new element of the backrefs list will be created
//	// and added as first element of backrefs block list
//	if( tmp == NULL || tmp->backrefs[N_BACKREFS - 1]) {
//		struct block_backrefs *blockRefs_to_insert = xdag_malloc(sizeof(struct block_backrefs));
//		if(blockRefs_to_insert == NULL) {
//			xdag_err("xdag_malloc failed. [function add_backref]");
//			return;
//		}
//		memset(blockRefs_to_insert, 0, sizeof(struct block_backrefs));
//		blockRefs_to_insert->next = tmp;
//		atomic_store_explicit_uintptr(&blockRef->backrefs, (uintptr_t)blockRefs_to_insert, memory_order_release);
//		tmp = blockRefs_to_insert;
//	}
//
//	// searching the first free array element
//	for(; tmp->backrefs[i]; ++i);
//	// adding the actual block memory address to the backrefs array
//	tmp->backrefs[i] = nodeBlock;
//}

//static inline int get_nfield(struct xdag_block *bref, int field_type)
//{
//	for(int i = 0; i < XDAG_BLOCK_FIELDS; ++i) {
//		if(xdag_type(bref, i) == field_type){
//			return i;
//		}
//	}
//	return -1;
//}

static inline const char* get_remark(struct block_internal *bi){
	if((bi->flags & BI_REMARK) & ~BI_EXTRA){
		const char* tmp = (const char*)atomic_load_explicit_uintptr(&bi->remark, memory_order_acquire);
		if(tmp != NULL){
			return tmp;
		} else if(load_remark(bi)){
			return (const char*)atomic_load_explicit_uintptr(&bi->remark, memory_order_relaxed);
		}
	}
	return "";
}

static int load_remark(struct block_internal* bi) {
    xdag_remark_t remark;
    if(!xdag_rsdb_get_remark(bi->hash, remark)) {
        xdag_err("Remark field not found [function: load_remark]");
        pthread_mutex_lock(&block_mutex);
        bi->flags &= ~BI_REMARK;
        pthread_mutex_unlock(&block_mutex);
        return 0;
    }
	return add_remark_bi(bi, remark);
}

//void order_ourblocks_by_amount(struct block_internal *bi)
//{
//    struct block_internal *ti = xdag_rsdb_get_ourbi(bi->hash);
//
//    //TODO use rocksdb key sort rule
////	while ((ti = bi->ourprev) && bi->amount > ti->amount) {
////		bi->ourprev = ti->ourprev;
////		ti->ournext = bi->ournext;
////		bi->ournext = ti;
////		ti->ourprev = bi;
////		*(bi->ourprev ? &bi->ourprev->ournext : &ourfirst) = bi;
////		*(ti->ournext ? &ti->ournext->ourprev : &ourlast) = ti;
////	}
//// 	while ((ti = bi->ournext) && bi->amount < ti->amount) {
////		bi->ournext = ti->ournext;
////		ti->ourprev = bi->ourprev;
////		bi->ourprev = ti;
////		ti->ournext = bi;
////		*(bi->ournext ? &bi->ournext->ourprev : &ourlast) = bi;
////		*(ti->ourprev ? &ti->ourprev->ournext : &ourfirst) = ti;
////	}
//    if(ti) free(ti);
//    ti = NULL;
// }

// add ourblock should only save hash of block_internal
static inline void add_ourblock(struct block_internal *nodeBlock)
{
//    nodeBlock->ourprev = ourlast;
    memcpy(nodeBlock->ourprev, ourlast_hash, sizeof(xdag_hashlow_t));

    struct block_internal ourlast;

//    *(ourlast ? &ourlast->ournext : &ourfirst) = nodeBlock;
    if(!xdag_rsdb_get_ourbi(ourlast_hash, &ourlast)) {
        memcpy(ourlast.ournext, nodeBlock->hash, sizeof(xdag_hashlow_t));
        xdag_rsdb_put_ourbi(&ourlast);
    } else {
        memcpy(ourfirst_hash, nodeBlock->hash, sizeof(xdag_hashlow_t));
        xdag_rsdb_put_setting(SETTING_OUR_FIRST_HASH, (const char*)ourfirst_hash, sizeof(ourfirst_hash));
    }
//    ourlast = nodeBlock;
    memcpy(ourlast_hash, nodeBlock->hash, sizeof(xdag_hashlow_t));
    xdag_rsdb_put_setting(SETTING_OUR_LAST_HASH, (const char*)ourlast_hash, sizeof(ourlast_hash));
    xdag_rsdb_put_ourbi(nodeBlock);
}

//static inline void remove_ourblock(struct block_internal *nodeBlock){
////    struct block_internal *prev = nodeBlock->ourprev, *next = nodeBlock->ournext;
//    struct block_internal* prev_bi = xdag_rsdb_get_ourbi(nodeBlock->ourprev);
//    struct block_internal* next_bi = xdag_rsdb_get_ourbi(nodeBlock->ournext);
//
//    //*(prev ? &prev->ournext : &ourfirst) = next;
//    if(prev_bi) {
//        memcpy(prev_bi->ournext, next_bi->hash, sizeof(xdag_hash_t));
//        xdag_rsdb_put_ourbi(prev_bi);
//
//    } else {
//        memcpy(ourfirst_hash, next_bi->hash, sizeof(xdag_hash_t));
//        char obf_hash[1] = {[0]=SETTING_OUR_FIRST_HASH};
//        xdag_rsdb_putkey(obf_hash, 1, (char*)ourfirst_hash, sizeof(xdag_hash_t));
//    }
//
//    //*(next ? &next->ourprev : &ourlast) = prev;
//    if(next_bi) {
//        memcpy(next_bi->ourprev, prev_bi->hash, sizeof(xdag_hash_t));
//        xdag_rsdb_put_ourbi(next_bi);
//
//    } else {
//        memcpy(ourlast_hash, prev_bi->hash, sizeof(xdag_hash_t));
//        char obl_hash[1] = {[0]=SETTING_OUR_LAST_HASH};
//        xdag_rsdb_putkey(obl_hash, 1, (char*)ourlast_hash, sizeof(xdag_hash_t));
//    }
//
//    xdag_rsdb_del_ourbi(nodeBlock->hash);
//}
