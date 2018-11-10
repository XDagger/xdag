/* block processing, T13.654-T14.618 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "system.h"
#include "../ldus/source/include/ldus/rbtree.h"
#include "block.h"
#include "crypt.h"
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

#define MAX_WAITING_MAIN        1
#define MAIN_START_AMOUNT       (1ll << 42)
#define MAIN_BIG_PERIOD_LOG     21
#define MAX_LINKS               15
#define MAKE_BLOCK_PERIOD       13

#define CACHE			1
#define CACHE_MAX_SIZE		600000
#define CACHE_MAX_SAMPLES	100
#define ORPHAN_HASH_SIZE	2
#define MAX_ALLOWED_EXTRA	0x10000

struct block_backrefs;
struct orphan_block;
struct block_internal_index;

struct block_internal {
	union {
		struct ldus_rbtree node;
		struct block_internal_index *index;
	};
	xdag_hash_t hash;
	xdag_diff_t difficulty;
	xdag_amount_t amount, linkamount[MAX_LINKS], fee;
	xtime_t time;
	uint64_t storage_pos;
	union {
		struct block_internal *ref;
		struct orphan_block *oref;
	};
	struct block_internal *link[MAX_LINKS];
	atomic_uintptr_t backrefs;
	atomic_uintptr_t remark;
	uint16_t flags, in_mask, n_our_key;
	uint8_t nlinks:4, max_diff_link:4, reserved;
};

struct block_internal_index {
	struct ldus_rbtree node;
	xdag_hash_t hash;
	struct block_internal *bi;
};

#define N_BACKREFS      (sizeof(struct block_internal) / sizeof(struct block_internal *) - 1)

struct block_backrefs {
	struct block_internal *backrefs[N_BACKREFS];
	struct block_backrefs *next;
};

#define ourprev link[MAX_LINKS - 2]
#define ournext link[MAX_LINKS - 1]

struct cache_block {
	struct ldus_rbtree node;
	xdag_hash_t hash;
	struct xdag_block block;
	struct cache_block *next;
};

struct orphan_block {
	struct block_internal *orphan_bi;
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

int g_bi_index_enable = 1, g_block_production_on;
static pthread_mutex_t g_create_block_mutex = PTHREAD_MUTEX_INITIALIZER;
static xdag_amount_t g_balance = 0;
extern xtime_t g_time_limit;
static struct ldus_rbtree *root = 0, *cache_root = 0;
static struct block_internal *volatile top_main_chain = 0, *volatile pretop_main_chain = 0;
static struct block_internal *ourfirst = 0, *ourlast = 0;
static struct cache_block *cache_first = NULL, *cache_last = NULL;
static pthread_mutex_t block_mutex;
static pthread_mutex_t rbtree_mutex;
//TODO: this variable duplicates existing global variable g_is_pool. Probably should be removed
static int g_light_mode = 0;
static uint32_t cache_bounded_counter = 0;
static struct orphan_block *g_orphan_first[ORPHAN_HASH_SIZE], *g_orphan_last[ORPHAN_HASH_SIZE];

//functions
void cache_retarget(int32_t, int32_t);
void cache_add(struct xdag_block*, xdag_hash_t);
int32_t check_signature_out_cached(struct block_internal*, struct xdag_public_key*, const int, int32_t*, int32_t*);
int32_t check_signature_out(struct block_internal*, struct xdag_public_key*, const int);
static int32_t find_and_verify_signature_out(struct xdag_block*, struct xdag_public_key*, const int);
int do_mining(struct xdag_block *block, struct block_internal **pretop, xtime_t send_time);
void remove_orphan(struct block_internal*,int);
void add_orphan(struct block_internal*,struct xdag_block*);
static inline size_t remark_acceptance(xdag_remark_t);
static int add_remark_bi(struct block_internal*, xdag_remark_t);
static void add_backref(struct block_internal*, struct block_internal*);
static inline int get_nfield(struct xdag_block*, int);
static inline const char* get_remark(struct block_internal*);
static int load_remark(struct block_internal*);
static void order_ourblocks_by_amount(struct block_internal *bi);
static inline void add_ourblock(struct block_internal *nodeBlock);
static inline void remove_ourblock(struct block_internal *nodeBlock);
void *add_block_callback(void *block, void *data);
extern void *sync_thread(void *arg);

static inline int lessthan(struct ldus_rbtree *l, struct ldus_rbtree *r)
{
	return memcmp(l + 1, r + 1, 24) < 0;
}

ldus_rbtree_define_prefix(lessthan, static inline, )

static inline struct block_internal *block_by_hash(const xdag_hashlow_t hash)
{
	if(g_bi_index_enable) {
		struct block_internal_index *index;
		pthread_mutex_lock(&rbtree_mutex);
		index = (struct block_internal_index *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
		pthread_mutex_unlock(&rbtree_mutex);
		return index ? index->bi : NULL;
	} else {
		struct block_internal *bi;
		pthread_mutex_lock(&rbtree_mutex);
		bi = (struct block_internal *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
		pthread_mutex_unlock(&rbtree_mutex);
		return bi;
	}
}

static inline struct cache_block *cache_block_by_hash(const xdag_hashlow_t hash)
{
	return (struct cache_block *)ldus_rbtree_find(cache_root, (struct ldus_rbtree *)hash - 1);
}


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
		order_ourblocks_by_amount(bi);
	}
}

static uint64_t apply_block(struct block_internal *bi)
{
	xdag_amount_t sum_in, sum_out;

	if (bi->flags & BI_MAIN_REF) {
		return -1l;
	}

	bi->flags |= BI_MAIN_REF;

	for (int i = 0; i < bi->nlinks; ++i) {
		xdag_amount_t ref_amount = apply_block(bi->link[i]);
		if (ref_amount == -1l) {
			continue;
		}
		bi->link[i]->ref = bi;
		if (bi->amount + ref_amount >= bi->amount) {
			accept_amount(bi, ref_amount);
		}
	}

	sum_in = 0, sum_out = bi->fee;

	for (int i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) {
			if (bi->link[i]->amount < bi->linkamount[i]) {
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

	if (sum_in + bi->amount < sum_in || sum_in + bi->amount < sum_out) {
		return 0;
	}

	for (int i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) {
			accept_amount(bi->link[i], (xdag_amount_t)0 - bi->linkamount[i]);
		} else {
			accept_amount(bi->link[i], bi->linkamount[i]);
		}
	}

	accept_amount(bi, sum_in - sum_out);
	bi->flags |= BI_APPLIED;

	return bi->fee;
}

static uint64_t unapply_block(struct block_internal *bi)
{
	int i;

	if (bi->flags & BI_APPLIED) {
		xdag_amount_t sum = bi->fee;

		for (i = 0; i < bi->nlinks; ++i) {
			if (1 << i & bi->in_mask) {
				accept_amount(bi->link[i], bi->linkamount[i]);
				sum -= bi->linkamount[i];
			} else {
				accept_amount(bi->link[i], (xdag_amount_t)0 - bi->linkamount[i]);
				sum += bi->linkamount[i];
			}
		}

		accept_amount(bi, sum);
		bi->flags &= ~BI_APPLIED;
	}

	bi->flags &= ~BI_MAIN_REF;
	bi->ref = 0;

	for (i = 0; i < bi->nlinks; ++i) {
		if (bi->link[i]->ref == bi && bi->link[i]->flags & BI_MAIN_REF) {
			accept_amount(bi, unapply_block(bi->link[i]));
		}
	}

	return (xdag_amount_t)0 - bi->fee;
}

// calculates current supply by specified count of main blocks
xdag_amount_t xdag_get_supply(uint64_t nmain)
{
	xdag_amount_t res = 0, amount = MAIN_START_AMOUNT;

	while (nmain >> MAIN_BIG_PERIOD_LOG) {
		res += (1l << MAIN_BIG_PERIOD_LOG) * amount;
		nmain -= 1l << MAIN_BIG_PERIOD_LOG;
		amount >>= 1;
	}
	res += nmain * amount;
	return res;
}

static void set_main(struct block_internal *m)
{
	xdag_amount_t amount = MAIN_START_AMOUNT >> (g_xdag_stats.nmain >> MAIN_BIG_PERIOD_LOG);

	m->flags |= BI_MAIN;
	accept_amount(m, amount);
	g_xdag_stats.nmain++;

	if (g_xdag_stats.nmain > g_xdag_stats.total_nmain) {
		g_xdag_stats.total_nmain = g_xdag_stats.nmain;
	}

	accept_amount(m, apply_block(m));
	m->ref = m;
	log_block((m->flags & BI_OURS ? "MAIN +" : "MAIN  "), m->hash, m->time, m->storage_pos);
}

static void unset_main(struct block_internal *m)
{
	g_xdag_stats.nmain--;
	g_xdag_stats.total_nmain--;
	xdag_amount_t amount = MAIN_START_AMOUNT >> (g_xdag_stats.nmain >> MAIN_BIG_PERIOD_LOG);
	m->flags &= ~BI_MAIN;
	accept_amount(m, (xdag_amount_t)0 - amount);
	accept_amount(m, unapply_block(m));
	log_block("UNMAIN", m->hash, m->time, m->storage_pos);
}

static void check_new_main(void)
{
	struct block_internal *b, *p = 0;
	int i;

	for (b = top_main_chain, i = 0; b && !(b->flags & BI_MAIN); b = b->link[b->max_diff_link]) {
		if (b->flags & BI_MAIN_CHAIN) {
			p = b;
			++i;
		}
	}

	if (p && (p->flags & BI_REF) && i > MAX_WAITING_MAIN && xdag_get_xtimestamp() >= p->time + 2 * 1024) {
		set_main(p);
	}
}

static void unwind_main(struct block_internal *b)
{
	for (struct block_internal *t = top_main_chain; t && t != b; t = t->link[t->max_diff_link]) {
		t->flags &= ~BI_MAIN_CHAIN;
		if (t->flags & BI_MAIN) {
			unset_main(t);
		}
	}
}

static inline void hash_for_signature(struct xdag_block b[2], const struct xdag_public_key *key, xdag_hash_t hash)
{
	memcpy((uint8_t*)(b + 1) + 1, (void*)((uintptr_t)key->pub & ~1l), sizeof(xdag_hash_t));

	*(uint8_t*)(b + 1) = ((uintptr_t)key->pub & 1) | 0x02;

	xdag_hash(b, sizeof(struct xdag_block) + sizeof(xdag_hash_t) + 1, hash);

	xdag_debug("Hash  : hash=[%s] data=[%s]", xdag_log_hash(hash),
		xdag_log_array(b, sizeof(struct xdag_block) + sizeof(xdag_hash_t) + 1));
}

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
				xdag_warn("Different result between openssl and secp256k1: res openssl=%2d res secp256k1=%2d key parity bit = %ld key=[%s] hash=[%s] r=[%s], s=[%s]",
					res2, res1, ((uintptr_t)keys[i].pub & 1), xdag_log_hash((uint64_t*)((uintptr_t)keys[i].pub & ~1l)),
					xdag_log_hash(hash), xdag_log_hash(b->field[signo_r].data), xdag_log_hash(b->field[signo_s].data));
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

static int remove_index(struct block_internal *bi)
{
	if(g_bi_index_enable) {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_remove(&root, &bi->index->node);
		pthread_mutex_unlock(&rbtree_mutex);
		free(bi->index);
		bi->index = NULL;
	} else {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_remove(&root, &bi->node);
		pthread_mutex_unlock(&rbtree_mutex);
	}
	return 0;
}

static int insert_index(struct block_internal *bi)
{
	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)malloc(sizeof(struct block_internal_index));
		if(!index) {
			xdag_err("block index malloc failed. [func: add_block_nolock]");
			return -1;
		}
		memset(index, 0, sizeof(struct block_internal_index));
		memcpy(index->hash, bi->hash, sizeof(xdag_hash_t));
		index->bi = bi;
		bi->index = index;

		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_insert(&root, &index->node);
		pthread_mutex_unlock(&rbtree_mutex);
	} else {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_insert(&root, &bi->node);
		pthread_mutex_unlock(&rbtree_mutex);
	}
	return 0;
}

#define set_pretop(b) if ((b) && MAIN_TIME((b)->time) < MAIN_TIME(timestamp) && \
		(!pretop_main_chain || xdag_diff_gt((b)->difficulty, pretop_main_chain->difficulty))) { \
		pretop_main_chain = (b); \
		log_block("Pretop", (b)->hash, (b)->time, (b)->storage_pos); \
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
	struct block_internal tmpNodeBlock, *blockRef = NULL, *blockRef0 = NULL;
	struct block_internal* blockRefs[XDAG_BLOCK_FIELDS-1]= {0};
	xdag_diff_t diff0, diff;
	int32_t cache_hit = 0, cache_miss = 0;

	memset(&tmpNodeBlock, 0, sizeof(struct block_internal));
	newBlock->field[0].transport_header = 0;
	xdag_hash(newBlock, sizeof(struct xdag_block), tmpNodeBlock.hash);

	if(block_by_hash(tmpNodeBlock.hash)) return 0;

	if(xdag_type(newBlock, 0) != g_block_header_type) {
		i = xdag_type(newBlock, 0);
		err = 1;
		goto end;
	}

	tmpNodeBlock.time = newBlock->field[0].time;

	if(tmpNodeBlock.time > timestamp + MAIN_CHAIN_PERIOD / 4 || tmpNodeBlock.time < XDAG_ERA
		|| (limit && timestamp - tmpNodeBlock.time > limit)) {
		i = 0;
		err = 2;
		goto end;
	}

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

	if(g_light_mode) {
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
			err = 0xC;
			goto end;
		}
	}

	/* if not read from storage and timestamp is ...ffff and last field is nonce then the block is extra */
	if (!g_light_mode && (transportHeader & (sizeof(struct xdag_block) - 1))
			&& (tmpNodeBlock.time & (MAIN_CHAIN_PERIOD - 1)) == (MAIN_CHAIN_PERIOD - 1)
			&& (signinmask & 1 << (XDAG_BLOCK_FIELDS - 1))) {
		tmpNodeBlock.flags |= BI_EXTRA;
	}

	for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (inmask | outmask)) {
			blockRefs[i-1] = block_by_hash(newBlock->field[i].hash);
			if(!blockRefs[i-1]) {
				err = 5;
				goto end;
			}
			if(blockRefs[i-1]->time >= tmpNodeBlock.time) {
				err = 6;
				goto end;
			}
			if(tmpNodeBlock.nlinks >= MAX_LINKS) {
				err = 7;
				goto end;
			}
		}
	}

	if(!g_light_mode) {
		check_new_main();
	}

	if(signOutCount) {
		our_keys = xdag_wallet_our_keys(&ourKeysCount);
	}

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

	for(i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (inmask | outmask)) {
			blockRef = blockRefs[i-1];
			if(1 << i & inmask) {
				if(newBlock->field[i].amount) {
					int32_t res = 1;
					if(CACHE) {
						res = check_signature_out_cached(blockRef, public_keys, keysCount, &cache_hit, &cache_miss);
					} else {
						res = check_signature_out(blockRef, public_keys, keysCount);
					}
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
			tmpNodeBlock.link[tmpNodeBlock.nlinks] = blockRef;
			tmpNodeBlock.linkamount[tmpNodeBlock.nlinks] = newBlock->field[i].amount;

			if(MAIN_TIME(blockRef->time) < MAIN_TIME(tmpNodeBlock.time)) {
				diff = xdag_diff_add(diff0, blockRef->difficulty);
			} else {
				diff = blockRef->difficulty;

				while(blockRef && MAIN_TIME(blockRef->time) == MAIN_TIME(tmpNodeBlock.time)) {
					blockRef = blockRef->link[blockRef->max_diff_link];
				}
				if(blockRef && xdag_diff_gt(xdag_diff_add(diff0, blockRef->difficulty), diff)) {
					diff = xdag_diff_add(diff0, blockRef->difficulty);
				}
			}

			if(xdag_diff_gt(diff, tmpNodeBlock.difficulty)) {
				tmpNodeBlock.difficulty = diff;
				tmpNodeBlock.max_diff_link = tmpNodeBlock.nlinks;
			}

			tmpNodeBlock.nlinks++;
		}
	}

	if(CACHE) {
		cache_retarget(cache_hit, cache_miss);
	}

	if(tmpNodeBlock.in_mask ? sum_in < sum_out : sum_out != newBlock->field[0].amount) {
		err = 0xB;
		goto end;
	}

	struct block_internal *nodeBlock;
	if (g_xdag_extstats.nextra > MAX_ALLOWED_EXTRA
			&& (g_xdag_state == XDAG_STATE_SYNC || g_xdag_state == XDAG_STATE_STST)) {
		/* if too many extra blocks then reuse the oldest */
		nodeBlock = g_orphan_first[1]->orphan_bi;
		remove_orphan(nodeBlock, ORPHAN_REMOVE_REUSE);
		remove_index(nodeBlock);
		if (g_xdag_stats.nblocks-- == g_xdag_stats.total_nblocks)
			g_xdag_stats.total_nblocks--;
		if (nodeBlock->flags & BI_OURS) {
			remove_ourblock(nodeBlock);
		}
	} else {
		nodeBlock = xdag_malloc(sizeof(struct block_internal));
	}
	if(!nodeBlock) {
		err = 0xC;
		goto end;
	}

	if(CACHE && signOutCount) {
		cache_add(newBlock, tmpNodeBlock.hash);
	}

	if(!(transportHeader & (sizeof(struct xdag_block) - 1))) {
		tmpNodeBlock.storage_pos = transportHeader;
	} else if (!(tmpNodeBlock.flags & BI_EXTRA)) {
		tmpNodeBlock.storage_pos = xdag_storage_save(newBlock);
	} else {
		/* do not store extra block right now */
		tmpNodeBlock.storage_pos = -2l;
	}

	memcpy(nodeBlock, &tmpNodeBlock, sizeof(struct block_internal));
	atomic_init_uintptr(&nodeBlock->backrefs, (uintptr_t)NULL);
	if(nodeBlock->flags & BI_REMARK){
		atomic_init_uintptr(&nodeBlock->remark, (uintptr_t)NULL);
	}

	if(!insert_index(nodeBlock)) {
		g_xdag_stats.nblocks++;
	} else {
		err = 0xC;
		goto end;
	}

	if(g_xdag_stats.nblocks > g_xdag_stats.total_nblocks) {
		g_xdag_stats.total_nblocks = g_xdag_stats.nblocks;
	}

	set_pretop(nodeBlock);
	set_pretop(top_main_chain);

	if(xdag_diff_gt(tmpNodeBlock.difficulty, g_xdag_stats.difficulty)) {
		/* Only log this if we are NOT loading state */
		if(g_xdag_state != XDAG_STATE_LOAD)
			xdag_info("Diff  : %llx%016llx (+%llx%016llx)", xdag_diff_args(tmpNodeBlock.difficulty), xdag_diff_args(diff0));

		for(blockRef = nodeBlock, blockRef0 = 0; blockRef && !(blockRef->flags & BI_MAIN_CHAIN); blockRef = blockRef->link[blockRef->max_diff_link]) {
			if((!blockRef->link[blockRef->max_diff_link] || xdag_diff_gt(blockRef->difficulty, blockRef->link[blockRef->max_diff_link]->difficulty))
				&& (!blockRef0 || MAIN_TIME(blockRef0->time) > MAIN_TIME(blockRef->time))) {
				blockRef->flags |= BI_MAIN_CHAIN;
				blockRef0 = blockRef;
			}
		}

		if(blockRef && blockRef0 && blockRef != blockRef0 && MAIN_TIME(blockRef->time) == MAIN_TIME(blockRef0->time)) {
			blockRef = blockRef->link[blockRef->max_diff_link];
		}

		unwind_main(blockRef);
		top_main_chain = nodeBlock;
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

	if(tmpNodeBlock.flags & BI_OURS) {
		add_ourblock(nodeBlock);
	}

	for(i = 0; i < tmpNodeBlock.nlinks; ++i) {
		remove_orphan(tmpNodeBlock.link[i],
				tmpNodeBlock.flags & BI_EXTRA ? ORPHAN_REMOVE_EXTRA : ORPHAN_REMOVE_NORMAL);

		if(tmpNodeBlock.linkamount[i]) {
			blockRef = tmpNodeBlock.link[i];
			add_backref(blockRef, nodeBlock);
		}
	}
	
	add_orphan(nodeBlock, newBlock);

	log_block((tmpNodeBlock.flags & BI_OURS ? "Good +" : "Good  "), tmpNodeBlock.hash, tmpNodeBlock.time, tmpNodeBlock.storage_pos);

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

end:
	for(j = 0; j < keysCount; ++j) {
		xdag_free_key(public_keys[j].key);
	}

	if(err > 0) {
		char buf[32] = {0};
		err |= i << 4;
		sprintf(buf, "Err %2x", err & 0xff);
		log_block(buf, tmpNodeBlock.hash, tmpNodeBlock.time, transportHeader);
	}

	return -err;
}

void *add_block_callback(void *block, void *data)
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

	if(res >= 0) {
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
	int i, j, res, mining, defkeynum, keysnum[XDAG_BLOCK_FIELDS], nkeys, nkeysnum = 0, outsigkeyind = -1, has_pool_tag = 0;
	struct xdag_public_key *defkey = xdag_wallet_default_key(&defkeynum), *keys = xdag_wallet_our_keys(&nkeys), *key;
	xdag_hash_t signatureHash;
	xdag_hash_t newBlockHash;
	struct block_internal *ref, *pretop = pretop_block();
	struct orphan_block *oref;

	for (i = 0; i < inputsCount; ++i) {
		ref = block_by_hash(fields[i].hash);
		if (!ref || !(ref->flags & BI_OURS)) {
			pthread_mutex_unlock(&g_create_block_mutex);
			return NULL;
		}

		for (j = 0; j < nkeysnum && ref->n_our_key != keysnum[j]; ++j);

		if (j == nkeysnum) {
			if (outsigkeyind < 0 && ref->n_our_key == defkeynum) {
				outsigkeyind = nkeysnum;
			}
			keysnum[nkeysnum++] = ref->n_our_key;
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

	if (g_light_mode) {
		pthread_mutex_lock(&g_create_block_mutex);
		if (res < XDAG_BLOCK_FIELDS && ourfirst) {
			setfld(XDAG_FIELD_OUT, ourfirst->hash, xdag_hashlow_t);
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
		for (oref = g_orphan_first[0]; oref && res < XDAG_BLOCK_FIELDS; oref = oref->next) {
			ref = oref->orphan_bi;
			if (ref->time < send_time) {
				setfld(XDAG_FIELD_OUT, ref->hash, xdag_hashlow_t);
				res++;
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

	log_block("Create", newBlockHash, block[0].field[0].time, 1);
	
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

	xdag_generate_random_array(block[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(xdag_hash_t));

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

static void reset_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = 0;

	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)node;
		bi = index->bi;
	} else {
		bi = (struct block_internal *)node;
	}

	struct block_backrefs *tmp;
	for(struct block_backrefs *to_free = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); to_free != NULL;){
		tmp = to_free->next;
		xdag_free(to_free);
		to_free = tmp;
	}
	if((bi->flags & BI_REMARK) && bi->remark != (uintptr_t)NULL) {
		xdag_free((char*)bi->remark);
	}
	xdag_free(bi);

	if(g_bi_index_enable) {
		free(node);
	}
}

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
	xdag_mess("Loading blocks from local storage...");

	xtime_t start = xdag_get_xtimestamp();
	xdag_show_state(0);

	xdag_load_blocks(t, xdag_get_xtimestamp(), &t, &add_block_callback);

	xdag_mess("Finish loading blocks, time cost %ldms", xdag_get_xtimestamp() - start);

	// waiting for command "run"
	while (!g_xdag_run) {
		g_xdag_state = XDAG_STATE_STOP;
		sleep(1);
	}

	// launching of synchronization thread
	g_xdag_sync_on = 1;
	if (!g_light_mode && !sync_thread_running) {
		xdag_mess("Starting sync thread...");
		int err = pthread_create(&th, 0, sync_thread, 0);
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

	if (g_light_mode) {
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

		if (!g_block_production_on && !g_light_mode &&
				(g_xdag_state == XDAG_STATE_WAIT || g_xdag_state == XDAG_STATE_WTST ||
				g_xdag_state == XDAG_STATE_SYNC || g_xdag_state == XDAG_STATE_STST || 
				g_xdag_state == XDAG_STATE_CONN || g_xdag_state == XDAG_STATE_CTST)) {
			if (g_xdag_state == XDAG_STATE_SYNC || g_xdag_state == XDAG_STATE_STST || 
					g_xdag_stats.nmain >= (MAIN_TIME(t) - xdag_start_main_time())) {
				g_block_production_on = 1;
			} else if (last_nmain != nmain) {
				last_nmain = nmain;
				last_time_nmain_unequal = time(NULL);
			} else if (time(NULL) - last_time_nmain_unequal > MAX_TIME_NMAIN_STALLED) {
				g_block_production_on = 1;
			}

			if (g_block_production_on) {
				xdag_mess("Starting refer blocks creation...");

				// start mining threads
				xdag_mess("Starting mining threads...");
				xdag_mining_start(n_mining_threads);
			}

		}

		if (g_block_production_on && 
				(nblk = (unsigned)g_xdag_extstats.nnoref / (XDAG_BLOCK_FIELDS - 5))) {
			nblk = nblk / 61 + (nblk % 61 > (unsigned)rand() % 61);

			while (nblk--) {
				xdag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
			}
		}

		pthread_mutex_lock(&block_mutex);

		if (g_xdag_state == XDAG_STATE_REST) {
			g_xdag_sync_on = 0;
			pthread_mutex_unlock(&block_mutex);
			xdag_mining_start(0);

			while (xdag_get_xtimestamp() - t < MAIN_CHAIN_PERIOD + (3 << 10)) {
				sleep(1);
			}

			pthread_mutex_lock(&block_mutex);

			if (xdag_free_all()) {
				pthread_mutex_lock(&rbtree_mutex);
				ldus_rbtree_walk_up(root, reset_callback);
				pthread_mutex_unlock(&rbtree_mutex);
			}
			
			root = 0;
			g_balance = 0;
			top_main_chain = pretop_main_chain = 0;
			ourfirst = ourlast = 0;
			g_orphan_first[0] = g_orphan_last[0] = 0;
			g_orphan_first[1] = g_orphan_last[1] = 0;
			memset(&g_xdag_stats, 0, sizeof(g_xdag_stats));
			memset(&g_xdag_extstats, 0, sizeof(g_xdag_extstats));
			pthread_mutex_unlock(&block_mutex);
			conn_time = sync_time = 0;

			goto begin;
		} else {
			pthread_mutex_lock(&g_transport_mutex);
			if (t > (g_xdag_last_received << 10) && t - (g_xdag_last_received << 10) > 3 * MAIN_CHAIN_PERIOD) {
				g_xdag_state = (g_light_mode ? (g_xdag_testnet ? XDAG_STATE_TTST : XDAG_STATE_TRYP)
					: (g_xdag_testnet ? XDAG_STATE_WTST : XDAG_STATE_WAIT));
				conn_time = sync_time = 0;
			} else {
				if (!conn_time) {
					conn_time = t;
				}

				if (!g_light_mode && t - conn_time >= 2 * MAIN_CHAIN_PERIOD
					&& !memcmp(&g_xdag_stats.difficulty, &g_xdag_stats.max_difficulty, sizeof(xdag_diff_t))) {
					sync_time = t;
				}

				if (t - (g_xdag_xfer_last << 10) <= 2 * MAIN_CHAIN_PERIOD + 4) {
					g_xdag_state = XDAG_STATE_XFER;
				} else if (g_light_mode) {
					g_xdag_state = (g_xdag_mining_threads > 0 ?
						(g_xdag_testnet ? XDAG_STATE_MTST : XDAG_STATE_MINE)
						: (g_xdag_testnet ? XDAG_STATE_PTST : XDAG_STATE_POOL));
				} else if (t - sync_time > 8 * MAIN_CHAIN_PERIOD) {
					g_xdag_state = (g_xdag_testnet ? XDAG_STATE_CTST : XDAG_STATE_CONN);
				} else {
					g_xdag_state = (g_xdag_testnet ? XDAG_STATE_STST : XDAG_STATE_SYNC);
				}
			}
			pthread_mutex_unlock(&g_transport_mutex);
		}

		if (!g_light_mode) {
			check_new_main();
		}

		struct block_internal *ours = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		xdag_show_state(ours ? ours->hash : 0);

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
int xdag_blocks_start(int is_pool, int mining_threads_count, int miner_address)
{
	pthread_mutexattr_t attr;
	pthread_t th;

	if (!is_pool) {
		g_light_mode = 1;
	}

	if (xdag_mem_init(g_light_mode && !miner_address ? 0 : (((xdag_get_xtimestamp() - XDAG_ERA) >> 10) + (uint64_t)365 * 24 * 60 * 60) * 2 * sizeof(struct block_internal))) {
		return -1;
	}

	g_bi_index_enable = g_use_tmpfile;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&block_mutex, &attr);
	pthread_mutex_init(&rbtree_mutex, 0);
	int err = pthread_create(&th, 0, work_thread, (void*)(uintptr_t)(unsigned)mining_threads_count);
	if(err != 0) {
		printf("create work_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	err = pthread_detach(th);
	if(err != 0) {
		printf("create pool_main_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	return 0;
}

/* returns our first block. If there is no blocks yet - the first block is created. */
int xdag_get_our_block(xdag_hash_t hash)
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = ourfirst;
	pthread_mutex_unlock(&block_mutex);

	if (!bi) {
		xdag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
		pthread_mutex_lock(&block_mutex);
		bi = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		if (!bi) {
			return -1;
		}
	}

	memcpy(hash, bi->hash, sizeof(xdag_hash_t));

	return 0;
}

/* calls callback for each own block */
int xdag_traverse_our_blocks(void *data,
    int (*callback)(void*, xdag_hash_t, xdag_amount_t, xtime_t, int))
{
	int res = 0;

	pthread_mutex_lock(&block_mutex);

	for (struct block_internal *bi = ourfirst; !res && bi; bi = bi->ournext) {
		res = (*callback)(data, bi->hash, bi->amount, bi->time, bi->n_our_key);
	}

	pthread_mutex_unlock(&block_mutex);

	return res;
}

static int (*g_traverse_callback)(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time);
static void *g_traverse_data;

static void traverse_all_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = 0;
	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)node;
		bi = index->bi;
	} else {
		bi = (struct block_internal *)node;
	}

	(*g_traverse_callback)(g_traverse_data, bi->hash, bi->amount, bi->time);
}

/* calls callback for each block */
int xdag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
	xdag_amount_t amount, xtime_t time))
{
	pthread_mutex_lock(&block_mutex);
	g_traverse_callback = callback;
	g_traverse_data = data;
	pthread_mutex_lock(&rbtree_mutex);
	ldus_rbtree_walk_right(root, traverse_all_callback);
	pthread_mutex_unlock(&rbtree_mutex);
	pthread_mutex_unlock(&block_mutex);
	return 0;
}

/* returns current balance for specified address or balance for all addresses if hash == 0 */
xdag_amount_t xdag_get_balance(xdag_hash_t hash)
{
	if (!hash) {
		return g_balance;
	}

	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		return 0;
	}

	return bi->amount;
}

/* sets current balance for the specified address */
int xdag_set_balance(xdag_hash_t hash, xdag_amount_t balance)
{
	if (!hash) return -1;

	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	if (bi->flags & BI_OURS && bi != ourfirst) {
		if (bi->ourprev) {
			bi->ourprev->ournext = bi->ournext;
		} else {
			ourfirst = bi->ournext;
		}

		if (bi->ournext) {
			bi->ournext->ourprev = bi->ourprev;
		} else {
			ourlast = bi->ourprev;
		}

		bi->ourprev = 0;
		bi->ournext = ourfirst;

		if (ourfirst) {
			ourfirst->ourprev = bi;
		} else {
			ourlast = bi;
		}

		ourfirst = bi;
	}

	pthread_mutex_unlock(&block_mutex);

	if (!bi) return -1;

	if (bi->amount != balance) {
		xdag_hash_t hash0;
		xdag_amount_t diff;

		memset(hash0, 0, sizeof(xdag_hash_t));

		if (balance > bi->amount) {
			diff = balance - bi->amount;
			xdag_log_xfer(hash0, hash, diff);
			if (bi->flags & BI_OURS) {
				g_balance += diff;
			}
		} else {
			diff = bi->amount - balance;
			xdag_log_xfer(hash, hash0, diff);
			if (bi->flags & BI_OURS) {
				g_balance -= diff;
			}
		}

		bi->amount = balance;
	}

	return 0;
}

// returns position and time of block by hash; if block is extra and block != 0 also returns the whole block
int64_t xdag_get_block_pos(const xdag_hash_t hash, xtime_t *t, struct xdag_block *block)
{
	if (block) pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		if (block) pthread_mutex_unlock(&block_mutex);
		return -1;
	}

	if (block && bi->flags & BI_EXTRA) {
		memcpy(block, bi->oref->block, sizeof(struct xdag_block));
	}

	if (block) pthread_mutex_unlock(&block_mutex);

	*t = bi->time;

	return bi->storage_pos;
}

//returns a number of key by hash of block, or -1 if block is not ours
int xdag_get_key(xdag_hash_t hash)
{
	struct block_internal *bi = block_by_hash(hash);

	if (!bi || !(bi->flags & BI_OURS)) {
		return -1;
	}

	return bi->n_our_key;
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

static int bi_compar(const void *l, const void *r)
{
	xtime_t tl = (*(struct block_internal **)l)->time, tr = (*(struct block_internal **)r)->time;

	return (tl < tr) - (tl > tr);
}

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

/* prints detailed information about block */
int xdag_print_block_info(xdag_hash_t hash, FILE *out)
{
	char time_buf[64] = {0};
	char address[33] = {0};
	int i;

	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		return -1;
	}

	uint64_t *h = bi->hash;
	xdag_xtime_to_string(bi->time, time_buf);
	fprintf(out, "      time: %s\n", time_buf);
	fprintf(out, " timestamp: %llx\n", (unsigned long long)bi->time);
	fprintf(out, "     flags: %x\n", bi->flags & ~BI_OURS);
	fprintf(out, "     state: %s\n", xdag_get_block_state_info(bi->flags));
	fprintf(out, "  file pos: %llx\n", (unsigned long long)bi->storage_pos);
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
	struct block_internal *ref;
	pthread_mutex_lock(&block_mutex);
	ref = bi->ref;
	flags = bi->flags;
	pthread_mutex_unlock(&block_mutex);
	if((flags & BI_REF) && ref != NULL) {
		xdag_hash2address(ref->hash, address);
	} else {
		strcpy(address, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
	}
	fprintf(out, "       fee: %s  %10u.%09u\n", address, pramount(bi->fee));

 	if(flags & BI_EXTRA) pthread_mutex_lock(&block_mutex);
 	int nlinks = bi->nlinks;
	struct block_internal *link[MAX_LINKS];
	memcpy(link, bi->link, nlinks * sizeof(struct block_internal*));
	if(flags & BI_EXTRA) pthread_mutex_unlock(&block_mutex);

 	for (i = 0; i < nlinks; ++i) {
		xdag_hash2address(link[i]->hash, address);
		fprintf(out, "    %6s: %s  %10u.%09u\n", (1 << i & bi->in_mask ? " input" : "output"),
			address, pramount(bi->linkamount[i]));
	}

	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "                                 block as address: details\n");
	fprintf(out, " direction  transaction                                amount       time                     remark                          \n");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");

	int N = 0x10000;
	int n = 0;
	struct block_internal **ba = malloc(N * sizeof(struct block_internal *));

	if (!ba) return -1;

	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);

		if (!i) {
			continue;
		}

		if (n + i > N) {
			N *= 2;
			struct block_internal **ba1 = realloc(ba, N * sizeof(struct block_internal *));
			if (!ba1) {
				free(ba);
				return -1;
			}

			ba = ba1;
		}

		memcpy(ba + n, br->backrefs, i * sizeof(struct block_internal *));
		n += i;
	}

	if (n) {
		qsort(ba, n, sizeof(struct block_internal *), bi_compar);

		for (i = 0; i < n; ++i) {
			if (!i || ba[i] != ba[i - 1]) {
				struct block_internal *ri = ba[i];
				if (ri->flags & BI_APPLIED) {
					for (int j = 0; j < ri->nlinks; j++) {
						if(ri->link[j] == bi && ri->linkamount[j]) {
							xdag_xtime_to_string(ri->time, time_buf);
							xdag_hash2address(ri->hash, address);
							fprintf(out, "    %6s: %s  %10u.%09u  %s  %s\n",
								(1 << j & ri->in_mask ? "output" : " input"), address,
								pramount(ri->linkamount[j]), time_buf, get_remark(ri));
						}
					}
				}
			}
		}
	}

	free(ba);
	
	if (bi->flags & BI_MAIN) {
		xdag_hash2address(h, address);
		fprintf(out, "   earning: %s  %10u.%09u  %s\n", address,
			pramount(MAIN_START_AMOUNT >> ((MAIN_TIME(bi->time) - MAIN_TIME(XDAG_ERA)) >> MAIN_BIG_PERIOD_LOG)),
			time_buf);
	}
	
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

	pthread_mutex_lock(&block_mutex);

	for (struct block_internal *b = top_main_chain; b && i < count; b = b->link[b->max_diff_link]) {
		if (b->flags & BI_MAIN) {
			print_block(b, print_only_addresses, out);
			++i;
		}
	}

	pthread_mutex_unlock(&block_mutex);
}

// prints list of N last blocks mined by current pool
// TODO: find a way to find non-payed mined blocks or remove 'include_non_payed' parameter
void xdag_list_mined_blocks(int count, int include_non_payed, FILE *out)
{
	int i = 0;
	print_header_block_list(out);

	pthread_mutex_lock(&block_mutex);

	for(struct block_internal *b = top_main_chain; b && i < count; b = b->link[b->max_diff_link]) {
		if(b->flags & BI_MAIN && b->flags & BI_OURS) {
			print_block(b, 0, out);
			++i;
		}
	}

	pthread_mutex_unlock(&block_mutex);
}

void cache_retarget(int32_t cache_hit, int32_t cache_miss)
{
	if(g_xdag_extstats.cache_usage >= g_xdag_extstats.cache_size) {
		if(g_xdag_extstats.cache_hitrate < 0.94 && g_xdag_extstats.cache_size < CACHE_MAX_SIZE) {
			g_xdag_extstats.cache_size++;
		} else if(g_xdag_extstats.cache_hitrate > 0.98 && !cache_miss && g_xdag_extstats.cache_size && (rand() & 0xF) < 0x5) {
			g_xdag_extstats.cache_size--;
		}
		for(int l = g_xdag_extstats.cache_usage; l > g_xdag_extstats.cache_size; l--) {
			if(cache_first != NULL) {
				struct cache_block* to_free = cache_first;
				cache_first = cache_first->next;
				if(cache_first == NULL) {
					cache_last = NULL;
				}
				ldus_rbtree_remove(&cache_root, &to_free->node);
				free(to_free);
				g_xdag_extstats.cache_usage--;
			} else {
				break;
				xdag_warn("Non critical error, break in for [function: cache_retarget]");
			}
		}

	} else if(g_xdag_extstats.cache_hitrate > 0.98 && !cache_miss && g_xdag_extstats.cache_size && (rand() & 0xF) < 0x5) {
		g_xdag_extstats.cache_size--;
	}
	if((uint32_t)(g_xdag_extstats.cache_size / 0.9) > CACHE_MAX_SIZE) {
		g_xdag_extstats.cache_size = (uint32_t)(g_xdag_extstats.cache_size*0.9);
	}
	if(cache_hit + cache_miss > 0) {
		if(cache_bounded_counter < CACHE_MAX_SAMPLES)
			cache_bounded_counter++;
		g_xdag_extstats.cache_hitrate = moving_average_double(g_xdag_extstats.cache_hitrate, (double)((cache_hit) / (cache_hit + cache_miss)), cache_bounded_counter);

	}
}

void cache_add(struct xdag_block* block, xdag_hash_t hash)
{
	if(g_xdag_extstats.cache_usage <= CACHE_MAX_SIZE) {
		struct cache_block *cacheBlock = malloc(sizeof(struct cache_block));
		if(cacheBlock != NULL) {
			memset(cacheBlock, 0, sizeof(struct cache_block));
			memcpy(&(cacheBlock->block), block, sizeof(struct xdag_block));
			memcpy(&(cacheBlock->hash), hash, sizeof(xdag_hash_t));

			if(cache_first == NULL)
				cache_first = cacheBlock;
			if(cache_last != NULL)
				cache_last->next = cacheBlock;
			cache_last = cacheBlock;
			ldus_rbtree_insert(&cache_root, &cacheBlock->node);
			g_xdag_extstats.cache_usage++;
		} else {
			xdag_warn("cache malloc failed [function: cache_add]");
		}
	} else {
		xdag_warn("maximum cache reached [function: cache_add]");
	}

}

int32_t check_signature_out_cached(struct block_internal* blockRef, struct xdag_public_key *public_keys, const int keysCount, int32_t *cache_hit, int32_t *cache_miss)
{
	struct cache_block *bref = cache_block_by_hash(blockRef->hash);
	if(bref != NULL) {
		++(*cache_hit);
		return  find_and_verify_signature_out(&(bref->block), public_keys, keysCount);
	} else {
		++(*cache_miss);
		return check_signature_out(blockRef, public_keys, keysCount);
	}
}

int32_t check_signature_out(struct block_internal* blockRef, struct xdag_public_key *public_keys, const int keysCount)
{
	struct xdag_block buf;
	struct xdag_block *bref = xdag_storage_load(blockRef->hash, blockRef->time, blockRef->storage_pos, &buf);
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
	struct block_internal *bi = block_by_hash(hash);
	
	if (!bi) {
		return -1;
	}
	
	int size = 0x10000; 
	int n = 0;
	struct block_internal **block_array = malloc(size * sizeof(struct block_internal *));
	
	if (!block_array) return -1;
	
	int i;
	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);
		
		if (!i) {
			continue;
		}
		
		if (n + i > size) {
			size *= 2;
			struct block_internal **tmp_array = realloc(block_array, size * sizeof(struct block_internal *));
			if (!tmp_array) {
				free(block_array);
				return -1;
			}
			
			block_array = tmp_array;
		}
		
		memcpy(block_array + n, br->backrefs, i * sizeof(struct block_internal *));
		n += i;
	}
	
	if (!n) {
		free(block_array);
		return 0;
	}
	
	qsort(block_array, n, sizeof(struct block_internal *), bi_compar);
	
	for (i = 0; i < n; ++i) {
		if (!i || block_array[i] != block_array[i - 1]) {
			struct block_internal *ri = block_array[i];
			for (int j = 0; j < ri->nlinks; j++) {
				if(ri->link[j] == bi && ri->linkamount[j]) {
					if(callback(data, 1 << j & ri->in_mask, ri->flags, ri->hash, ri->linkamount[j], ri->time, get_remark(ri))) {
						free(block_array);
						return n;
					}
				}
			}
		}
	}
	
	free(block_array);
	
	return n;
}

void remove_orphan(struct block_internal* bi, int remove_action)
{
	if(!(bi->flags & BI_REF) && (remove_action != ORPHAN_REMOVE_EXTRA || (bi->flags & BI_EXTRA))) {
		struct orphan_block *obt = bi->oref;
		if (obt == NULL) {
			xdag_crit("Critical error. obt=0");
		} else if (obt->orphan_bi != bi) {
			xdag_crit("Critical error. bi=%p, flags=%x, action=%d, obt=%p, prev=%p, next=%p, obi=%p",
				  bi, bi->flags, remove_action, obt, obt->prev, obt->next, obt->orphan_bi);
		} else {
			int index = get_orphan_index(bi), i;
			struct orphan_block *prev = obt->prev, *next = obt->next;

			*(prev ? &(prev->next) : &g_orphan_first[index]) = next;
			*(next ? &(next->prev) : &g_orphan_last[index]) = prev;

			if (index) {
				if (remove_action != ORPHAN_REMOVE_REUSE) {
					bi->storage_pos = xdag_storage_save(obt->block);
					for (i = 0; i < bi->nlinks; ++i) {
						remove_orphan(bi->link[i], ORPHAN_REMOVE_NORMAL);
					}
				}
				bi->flags &= ~BI_EXTRA;
				g_xdag_extstats.nextra--;
			} else {
				g_xdag_extstats.nnoref--;
			}

			bi->oref = 0;
			bi->flags |= BI_REF;
			free(obt);
		}
	}
}

void add_orphan(struct block_internal* bi, struct xdag_block *block)
{
	int index = get_orphan_index(bi);
	struct orphan_block *obt = malloc(sizeof(struct orphan_block) + index * sizeof(struct xdag_block));
	if(obt == NULL){
		xdag_crit("Error. Malloc failed. [function: add_orphan]");
	} else {
		obt->orphan_bi = bi;
		obt->prev = g_orphan_last[index];
		obt->next = 0;
		bi->oref = obt;
		*(g_orphan_last[index] ? &g_orphan_last[index]->next : &g_orphan_first[index]) = obt;
		g_orphan_last[index] = obt;
		if (index) {
			memcpy(obt->block, block, sizeof(struct xdag_block));
			g_xdag_extstats.nextra++;
		} else {
			g_xdag_extstats.nnoref++;
		}
	}
}

void xdag_list_orphan_blocks(int count, FILE *out)
{
	int i = 0;
	print_header_block_list(out);

	pthread_mutex_lock(&block_mutex);

	for(struct orphan_block *b = g_orphan_first[0]; b && i < count; b = b->next, i++) {
		print_block(b->orphan_bi, 0, out);
	}

	pthread_mutex_unlock(&block_mutex);
}

// completes work with the blocks
void xdag_block_finish()
{
	pthread_mutex_lock(&g_create_block_mutex);
	pthread_mutex_lock(&block_mutex);
}

int xdag_get_block_info(xdag_hash_t hash, void *info, int (*info_callback)(void*, int, xdag_hash_t, xdag_amount_t, xtime_t, const char *),
						void *links, int (*links_callback)(void*, const char *, xdag_hash_t, xdag_amount_t))
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);

	if(info_callback && bi) {
		info_callback(info, bi->flags & ~BI_OURS,  bi->hash, bi->amount, bi->time, get_remark(bi));
	}

	if(links_callback && bi) {
		int flags;
		struct block_internal *ref;
		pthread_mutex_lock(&block_mutex);
		ref = bi->ref;
		flags = bi->flags;
		pthread_mutex_unlock(&block_mutex);

		xdag_hash_t link_hash;
		memset(link_hash, 0, sizeof(xdag_hash_t));
		if((flags & BI_REF) && ref != NULL) {
			memcpy(link_hash, ref->hash, sizeof(xdag_hash_t));
		}
		links_callback(links, "fee", link_hash, bi->fee);

		struct block_internal *bi_links[MAX_LINKS] = {0};
		int bi_nlinks = 0;

		if(flags & BI_EXTRA) {
			pthread_mutex_lock(&block_mutex);
		}

		bi_nlinks = bi->nlinks;
		memcpy(bi_links, bi->link, bi_nlinks * sizeof(struct block_internal *));

		if(flags & BI_EXTRA) {
			pthread_mutex_unlock(&block_mutex);
		}

		for (int i = 0; i < bi_nlinks; ++i) {
			links_callback(links, (1 << i & bi->in_mask ? " input" : "output"), bi_links[i]->hash, bi->linkamount[i]);
		}
	}
	return 0;
}

static inline size_t remark_acceptance(xdag_remark_t origin)
{
	char remark_buf[33] = {0};
	memcpy(remark_buf, origin, sizeof(xdag_remark_t));
	size_t size = validate_remark(remark_buf);
	if(size){
		return size;
	}
	return 0;
}

static int add_remark_bi(struct block_internal* bi, xdag_remark_t strbuf)
{
	size_t size = remark_acceptance(strbuf);
	char *remark_tmp = xdag_malloc(size + 1);
	if(remark_tmp == NULL) {
		xdag_err("xdag_malloc failed, [function add_remark_bi]");
		return 0;
	}
	memset(remark_tmp, 0, size + 1);
	memcpy(remark_tmp, strbuf, size);
	uintptr_t expected_value = 0 ;
	if(!atomic_compare_exchange_strong_explicit_uintptr(&bi->remark, &expected_value, (uintptr_t)remark_tmp, memory_order_acq_rel, memory_order_relaxed)){
		free(remark_tmp);
	}
	return 1;
}

static void add_backref(struct block_internal* blockRef, struct block_internal* nodeBlock)
{
	int i = 0;

	struct block_backrefs *tmp = (struct block_backrefs*)atomic_load_explicit_uintptr(&blockRef->backrefs, memory_order_acquire);
	// LIFO list: if the first element doesn't exist or it is full, a new element of the backrefs list will be created
	// and added as first element of backrefs block list
	if( tmp == NULL || tmp->backrefs[N_BACKREFS - 1]) {
		struct block_backrefs *blockRefs_to_insert = xdag_malloc(sizeof(struct block_backrefs));
		if(blockRefs_to_insert == NULL) {
			xdag_err("xdag_malloc failed. [function add_backref]");
			return;
		}
		memset(blockRefs_to_insert, 0, sizeof(struct block_backrefs));
		blockRefs_to_insert->next = tmp;
		atomic_store_explicit_uintptr(&blockRef->backrefs, (uintptr_t)blockRefs_to_insert, memory_order_release);
		tmp = blockRefs_to_insert;
	}

	// searching the first free array element
	for(; tmp->backrefs[i]; ++i);
	// adding the actual block memory address to the backrefs array
	tmp->backrefs[i] = nodeBlock;
}

static inline int get_nfield(struct xdag_block *bref, int field_type)
{
	for(int i = 0; i < XDAG_BLOCK_FIELDS; ++i) {
		if(xdag_type(bref, i) == field_type){
			return i;
		}
	}
	return -1;
}

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
	struct xdag_block buf;
	struct xdag_block *bref = xdag_storage_load(bi->hash, bi->time, bi->storage_pos, &buf);
	if(bref == NULL) {
		return 0;
	}

	int remark_field = get_nfield(bref, XDAG_FIELD_REMARK);
	if (remark_field < 0) {
		xdag_err("Remark field not found [function: load_remark]");
		pthread_mutex_lock(&block_mutex);
		bi->flags &= ~BI_REMARK;
		pthread_mutex_unlock(&block_mutex);
		return 0;
	}
	return add_remark_bi(bi, bref->field[remark_field].remark);
}

void order_ourblocks_by_amount(struct block_internal *bi)
{
	struct block_internal *ti;
	while ((ti = bi->ourprev) && bi->amount > ti->amount) {
		bi->ourprev = ti->ourprev;
		ti->ournext = bi->ournext;
		bi->ournext = ti;
		ti->ourprev = bi;
		*(bi->ourprev ? &bi->ourprev->ournext : &ourfirst) = bi;
		*(ti->ournext ? &ti->ournext->ourprev : &ourlast) = ti;
	}
 	while ((ti = bi->ournext) && bi->amount < ti->amount) {
		bi->ournext = ti->ournext;
		ti->ourprev = bi->ourprev;
		bi->ourprev = ti;
		ti->ournext = bi;
		*(bi->ournext ? &bi->ournext->ourprev : &ourlast) = bi;
		*(ti->ourprev ? &ti->ourprev->ournext : &ourfirst) = ti;
	}
 }

static inline void add_ourblock(struct block_internal *nodeBlock)
{
	nodeBlock->ourprev = ourlast;
	*(ourlast ? &ourlast->ournext : &ourfirst) = nodeBlock;
	ourlast = nodeBlock;
}

static inline void remove_ourblock(struct block_internal *nodeBlock){
	struct block_internal *prev = nodeBlock->ourprev, *next = nodeBlock->ournext;
	*(prev ? &prev->ournext : &ourfirst) = next;
	*(next ? &next->ourprev : &ourlast) = prev;
}
