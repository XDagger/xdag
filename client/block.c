/* block processing, T13.654-T13.895 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "system.h"
#include "../ldus/source/include/ldus/rbtree.h"
#include "block.h"
#include "crypt.h"
#include "wallet.h"
#include "storage.h"
#include "transport.h"
#include "log.h"
#include "init.h"
#include "sync.h"
#include "pool.h"
#include "memory.h"
#include "address.h"
#include "commands.h"

#define MAIN_CHAIN_PERIOD       (64 << 10)
#define MAX_WAITING_MAIN        1
#define DEF_TIME_LIMIT          0 // (MAIN_CHAIN_PERIOD / 2)
#define XDAG_TEST_ERA           0x16900000000ll
#define XDAG_MAIN_ERA           0x16940000000ll
#define XDAG_ERA                xdag_era
#define MAIN_START_AMOUNT       (1ll << 42)
#define MAIN_BIG_PERIOD_LOG     21
#define MAIN_TIME(t)            ((t) >> 16)
#define MAX_LINKS               15
#define MAKE_BLOCK_PERIOD       13
#define QUERY_RETRIES           2

enum bi_flags {
	BI_MAIN         = 0x01,
	BI_MAIN_CHAIN   = 0x02,
	BI_APPLIED      = 0x04,
	BI_MAIN_REF     = 0x08,
	BI_REF          = 0x10,
	BI_OURS         = 0x20,
};

struct block_backrefs;

struct block_internal {
	struct ldus_rbtree node;
	xdag_hash_t hash;
	xdag_diff_t difficulty;
	xdag_amount_t amount, linkamount[MAX_LINKS], fee;
	xdag_time_t time;
	uint64_t storage_pos;
	struct block_internal *ref, *link[MAX_LINKS];
	struct block_backrefs *backrefs;
	uint8_t flags, nlinks, max_diff_link, reserved;
	uint16_t in_mask;
	uint16_t n_our_key;
};

#define N_BACKREFS      (sizeof(struct block_internal) / sizeof(struct block_internal *) - 1)

struct block_backrefs {
	struct block_internal *backrefs[N_BACKREFS];
	struct block_backrefs *next;
};

#define ourprev link[MAX_LINKS - 2]
#define ournext link[MAX_LINKS - 1]

static xdag_amount_t g_balance = 0;
static xdag_time_t time_limit = DEF_TIME_LIMIT, xdag_era = XDAG_MAIN_ERA;
static struct ldus_rbtree *root = 0;
static struct block_internal * volatile top_main_chain = 0, *volatile pretop_main_chain = 0;
static struct block_internal *ourfirst = 0, *ourlast = 0, *noref_first = 0, *noref_last = 0;
static pthread_mutex_t block_mutex;
static int g_light_mode = 0;

static uint64_t get_timestamp(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

// returns a time period index, where a period is 64 seconds long
xdag_time_t xdag_main_time(void)
{
	return MAIN_TIME(get_timestamp());
}

// returns the time period index corresponding to the start of the network
xdag_time_t xdag_start_main_time(void)
{
	return MAIN_TIME(XDAG_ERA);
}

static inline int lessthan(struct ldus_rbtree *l, struct ldus_rbtree *r)
{
	return memcmp(l + 1, r + 1, 24) < 0;
}

ldus_rbtree_define_prefix(lessthan, static inline, )

static struct block_internal *block_by_hash(const xdag_hashlow_t hash)
{
	return (struct block_internal *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
}

static void log_block(const char *mess, xdag_hash_t h, xdag_time_t t, uint64_t pos)
{
	xdag_info("%s: %016llx%016llx%016llx%016llx t=%llx pos=%llx", mess,
		((uint64_t*)h)[3], ((uint64_t*)h)[2], ((uint64_t*)h)[1], ((uint64_t*)h)[0], t, pos);
}

static inline void accept_amount(struct block_internal *bi, xdag_amount_t sum)
{
	if (!sum) {
		return;
	}

	bi->amount += sum;
	if (bi->flags & BI_OURS) {
		struct block_internal *ti;
		g_balance += sum;

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

	if (p && i > MAX_WAITING_MAIN && get_timestamp() >= p->time + 2 * 1024) {
		set_main(p);
	}
}

static void unwind_main(struct block_internal *b)
{
	for (struct block_internal *t = top_main_chain; t != b; t = t->link[t->max_diff_link]) {
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

static inline xdag_diff_t hash_difficulty(xdag_hash_t hash)
{
	xdag_diff_t res = ((xdag_diff_t*)hash)[1], max = xdag_diff_max;

	xdag_diff_shr32(&res);
	
	return xdag_diff_div(max, res);
}

// returns a number of public key from 'keys' array with lengh 'keysLength', which conforms to the signature starting from field signo_r of the block b
// returns -1 if nothing is found
static int valid_signature(const struct xdag_block *b, int signo_r, int keysLength, struct xdag_public_key *keys)
{
	struct xdag_block buf[2];
	xdag_hash_t hash;
	int i, signo_s = -1;

	memcpy(buf, b, sizeof(struct xdag_block));

	for (i = signo_r; i < XDAG_BLOCK_FIELDS; ++i) {
		if (xdag_type(b, i) == XDAG_FIELD_SIGN_IN || xdag_type(b, i) == XDAG_FIELD_SIGN_OUT) {
			memset(&buf[0].field[i], 0, sizeof(struct xdag_field));
			if (i > signo_r && signo_s < 0 && xdag_type(b, i) == xdag_type(b, signo_r)) {
				signo_s = i;
			}
		}
	}

	if (signo_s >= 0) {
		for (i = 0; i < keysLength; ++i) {
			hash_for_signature(buf, keys + i, hash);
			if (!xdag_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data)) {
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

/* checks and adds a new block to the storage
 * returns:
 *		>0 = block was added
 *		0  = block exists
 *		<0 = error
 */
static int add_block_nolock(struct xdag_block *newBlock, xdag_time_t limit)
{
	const uint64_t timestamp = get_timestamp();
	uint64_t sum_in = 0, sum_out = 0, *psum;
	const uint64_t transportHeader = newBlock->field[0].transport_header;
	struct xdag_public_key public_keys[16], *our_keys = 0;
	int i, j, k;
	int keysCount = 0, ourKeysCount = 0;
	int signInCount = 0, signOutCount = 0;
	int signinmask = 0, signoutmask = 0;
	int inmask = 0, outmask = 0;
	int verified_keys_mask = 0, err, type;
	struct block_internal tmpNodeBlock, *blockRef, *blockRef0;
	xdag_diff_t diff0, diff;

	memset(&tmpNodeBlock, 0, sizeof(struct block_internal));
	newBlock->field[0].transport_header = 0;
	xdag_hash(newBlock, sizeof(struct xdag_block), tmpNodeBlock.hash);

	if (block_by_hash(tmpNodeBlock.hash)) return 0;
	
	if (xdag_type(newBlock, 0) != XDAG_FIELD_HEAD) {
		i = xdag_type(newBlock, 0);
		err = 1;
		goto end;
	}

	tmpNodeBlock.time = newBlock->field[0].time;

	if (tmpNodeBlock.time > timestamp + MAIN_CHAIN_PERIOD / 4 || tmpNodeBlock.time < XDAG_ERA
		|| (limit && timestamp - tmpNodeBlock.time > limit)) {
		i = 0;
		err = 2;
		goto end;
	}

	if (!g_light_mode) {
		check_new_main();
	}

	for (i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		switch ((type = xdag_type(newBlock, i))) {
		case XDAG_FIELD_NONCE:
			break;
		case XDAG_FIELD_IN:
			inmask |= 1 << i;
			break;
		case XDAG_FIELD_OUT:
			outmask |= 1 << i;
			break;
		case XDAG_FIELD_SIGN_IN:
			if (++signInCount & 1) {
				signinmask |= 1 << i;
			}
			break;
		case XDAG_FIELD_SIGN_OUT:
			if (++signOutCount & 1) {
				signoutmask |= 1 << i;
			}
			break;
		case XDAG_FIELD_PUBLIC_KEY_0:
		case XDAG_FIELD_PUBLIC_KEY_1:
			if ((public_keys[keysCount].key = xdag_public_to_key(newBlock->field[i].data, type - XDAG_FIELD_PUBLIC_KEY_0))) {
				public_keys[keysCount++].pub = (uint64_t*)((uintptr_t)&newBlock->field[i].data | (type - XDAG_FIELD_PUBLIC_KEY_0));
			}
			break;
		default:
			err = 3;
			goto end;
		}
	}

	if (g_light_mode) {
		outmask = 0;
	}

	if (signOutCount & 1) {
		i = signOutCount;
		err = 4;
		goto end;
	}

	if (signOutCount) {
		our_keys = xdag_wallet_our_keys(&ourKeysCount);
	}

	for (i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		if (1 << i & (signinmask | signoutmask)) {
			int keyNumber = valid_signature(newBlock, i, keysCount, public_keys);
			if (keyNumber >= 0) {
				verified_keys_mask |= 1 << keyNumber;
			}
			if (1 << i & signoutmask && !(tmpNodeBlock.flags & BI_OURS) && (keyNumber = valid_signature(newBlock, i, ourKeysCount, our_keys)) >= 0) {
				tmpNodeBlock.flags |= BI_OURS;
				tmpNodeBlock.n_our_key = keyNumber;
			}
		}
	}

	for (i = j = 0; i < keysCount; ++i) {
		if (1 << i & verified_keys_mask) {
			if (i != j) {
				xdag_free_key(public_keys[j].key);
			}
			memcpy(public_keys + j++, public_keys + i, sizeof(struct xdag_public_key));
		}
	}

	keysCount = j;
	tmpNodeBlock.difficulty = diff0 = hash_difficulty(tmpNodeBlock.hash);
	sum_out += newBlock->field[0].amount;
	tmpNodeBlock.fee = newBlock->field[0].amount;

	for (i = 1; i < XDAG_BLOCK_FIELDS; ++i) {
		if (1 << i & (inmask | outmask)) {
			blockRef = block_by_hash(newBlock->field[i].hash);
			if (!blockRef) {
				err = 5;
				goto end;
			}
			if (blockRef->time >= tmpNodeBlock.time) {
				err = 6;
				goto end;
			}
			if (tmpNodeBlock.nlinks >= MAX_LINKS) {
				err = 7;
				goto end;
			}
			if (1 << i & inmask) {
				if (newBlock->field[i].amount) {
					struct xdag_block buf;
					struct xdag_block *bref = xdag_storage_load(blockRef->hash, blockRef->time, blockRef->storage_pos, &buf);
					if (!bref) {
						err = 8;
						goto end;
					}

					for (j = k = 0; j < XDAG_BLOCK_FIELDS; ++j) {
						if (xdag_type(bref, j) == XDAG_FIELD_SIGN_OUT && (++k & 1)
							&& valid_signature(bref, j, keysCount, public_keys) >= 0) {
							break;
						}
					}
					if (j == XDAG_BLOCK_FIELDS) {
						err = 9;
						goto end;
					}
				}
				psum = &sum_in;
				tmpNodeBlock.in_mask |= 1 << tmpNodeBlock.nlinks;
			} else {
				psum = &sum_out;
			}

			if (*psum + newBlock->field[i].amount < *psum) {
				err = 0xA;
				goto end;
			}

			*psum += newBlock->field[i].amount;
			tmpNodeBlock.link[tmpNodeBlock.nlinks] = blockRef;
			tmpNodeBlock.linkamount[tmpNodeBlock.nlinks] = newBlock->field[i].amount;

			if (MAIN_TIME(blockRef->time) < MAIN_TIME(tmpNodeBlock.time)) {
				diff = xdag_diff_add(diff0, blockRef->difficulty);
			} else {
				diff = blockRef->difficulty;

				while (blockRef && MAIN_TIME(blockRef->time) == MAIN_TIME(tmpNodeBlock.time)) {
					blockRef = blockRef->link[blockRef->max_diff_link];
				}
				if (blockRef && xdag_diff_gt(xdag_diff_add(diff0, blockRef->difficulty), diff)) {
					diff = xdag_diff_add(diff0, blockRef->difficulty);
				}
			}

			if (xdag_diff_gt(diff, tmpNodeBlock.difficulty)) {
				tmpNodeBlock.difficulty = diff;
				tmpNodeBlock.max_diff_link = tmpNodeBlock.nlinks;
			}

			tmpNodeBlock.nlinks++;
		}
	}

	if (tmpNodeBlock.in_mask ? sum_in < sum_out : sum_out != newBlock->field[0].amount) {
		err = 0xB;
		goto end;
	}

	struct block_internal *nodeBlock = xdag_malloc(sizeof(struct block_internal));

	if (!nodeBlock) {
		err = 0xC; 
		goto end;
	}

	if (!(transportHeader & (sizeof(struct xdag_block) - 1))) {
		tmpNodeBlock.storage_pos = transportHeader;
	} else {
		tmpNodeBlock.storage_pos = xdag_storage_save(newBlock);
	}
	
	memcpy(nodeBlock, &tmpNodeBlock, sizeof(struct block_internal));
	ldus_rbtree_insert(&root, &nodeBlock->node);
	g_xdag_stats.nblocks++;
	
	if (g_xdag_stats.nblocks > g_xdag_stats.total_nblocks) {
		g_xdag_stats.total_nblocks = g_xdag_stats.nblocks;
	}
	
	set_pretop(nodeBlock);
	set_pretop(top_main_chain);
	
	if (xdag_diff_gt(tmpNodeBlock.difficulty, g_xdag_stats.difficulty)) {
		xdag_info("Diff  : %llx%016llx (+%llx%016llx)", xdag_diff_args(tmpNodeBlock.difficulty), xdag_diff_args(diff0));

		for (blockRef = nodeBlock, blockRef0 = 0; blockRef && !(blockRef->flags & BI_MAIN_CHAIN); blockRef = blockRef->link[blockRef->max_diff_link]) {
			if ((!blockRef->link[blockRef->max_diff_link] || xdag_diff_gt(blockRef->difficulty, blockRef->link[blockRef->max_diff_link]->difficulty))
				&& (!blockRef0 || MAIN_TIME(blockRef0->time) > MAIN_TIME(blockRef->time))) {
				blockRef->flags |= BI_MAIN_CHAIN; 
				blockRef0 = blockRef;
			}
		}

		if (blockRef && blockRef0 && blockRef != blockRef0 && MAIN_TIME(blockRef->time) == MAIN_TIME(blockRef0->time)) {
			blockRef = blockRef->link[blockRef->max_diff_link];
		}

		unwind_main(blockRef);
		top_main_chain = nodeBlock;
		g_xdag_stats.difficulty = tmpNodeBlock.difficulty;

		if (xdag_diff_gt(g_xdag_stats.difficulty, g_xdag_stats.max_difficulty)) {
			g_xdag_stats.max_difficulty = g_xdag_stats.difficulty;
		}
	}

	if (tmpNodeBlock.flags & BI_OURS) {
		nodeBlock->ourprev = ourlast;
		*(ourlast ? &ourlast->ournext : &ourfirst) = nodeBlock;
		ourlast = nodeBlock;
	}

	for (i = 0; i < tmpNodeBlock.nlinks; ++i) {
		if (!(tmpNodeBlock.link[i]->flags & BI_REF)) {
			for (blockRef0 = 0, blockRef = noref_first; blockRef != tmpNodeBlock.link[i]; blockRef0 = blockRef, blockRef = blockRef->ref) {
				;
			}

			*(blockRef0 ? &blockRef0->ref : &noref_first) = blockRef->ref;

			if (blockRef == noref_last) {
				noref_last = blockRef0;
			}

			tmpNodeBlock.link[i]->flags |= BI_REF;
			g_xdag_extstats.nnoref--;
		}

		if (tmpNodeBlock.linkamount[i]) {
			blockRef = tmpNodeBlock.link[i];
			if (!blockRef->backrefs || blockRef->backrefs->backrefs[N_BACKREFS - 1]) {
				struct block_backrefs *back = xdag_malloc(sizeof(struct block_backrefs));
				if (!back) continue;
				memset(back, 0, sizeof(struct block_backrefs));
				back->next = blockRef->backrefs;
				blockRef->backrefs = back;
			}

			for (j = 0; blockRef->backrefs->backrefs[j]; ++j);

			blockRef->backrefs->backrefs[j] = nodeBlock;
		}
	}

	*(noref_last ? &noref_last->ref : &noref_first) = nodeBlock;
	noref_last = nodeBlock;
	g_xdag_extstats.nnoref++;
	
	log_block((tmpNodeBlock.flags & BI_OURS ? "Good +" : "Good  "), tmpNodeBlock.hash, tmpNodeBlock.time, tmpNodeBlock.storage_pos);
	
	i = MAIN_TIME(nodeBlock->time) & (HASHRATE_LAST_MAX_TIME - 1);
	if (MAIN_TIME(nodeBlock->time) > MAIN_TIME(g_xdag_extstats.hashrate_last_time)) {
		memset(g_xdag_extstats.hashrate_total + i, 0, sizeof(xdag_diff_t));
		memset(g_xdag_extstats.hashrate_ours + i, 0, sizeof(xdag_diff_t));
		g_xdag_extstats.hashrate_last_time = nodeBlock->time;
	}
	
	if (xdag_diff_gt(diff0, g_xdag_extstats.hashrate_total[i])) {
		g_xdag_extstats.hashrate_total[i] = diff0;
	}
	
	if (tmpNodeBlock.flags & BI_OURS && xdag_diff_gt(diff0, g_xdag_extstats.hashrate_ours[i])) {
		g_xdag_extstats.hashrate_ours[i] = diff0;
	}
	
	err = -1;
 
end:
	for (j = 0; j < keysCount; ++j) {
		xdag_free_key(public_keys[j].key);
	}

	if (err > 0) {
		char buf[32];
		err |= i << 4;
		sprintf(buf, "Err %2x", err & 0xff);
		log_block(buf, tmpNodeBlock.hash, tmpNodeBlock.time, transportHeader);
	}

	return -err;
}

static void *add_block_callback(void *block, void *data)
{
	struct xdag_block *b = (struct xdag_block *)block;
	xdag_time_t *t = (xdag_time_t*)data;
	int res;

	pthread_mutex_lock(&block_mutex);
	
	if (*t < XDAG_ERA) {
		(res = add_block_nolock(b, *t));
	} else if ((res = add_block_nolock(b, 0)) >= 0 && b->field[0].time > *t) {
		*t = b->field[0].time;
	}

	pthread_mutex_unlock(&block_mutex);
	
	if (res >= 0) {
		xdag_sync_pop_block(b);
	}

	return 0;
}

/* checks and adds block to the storage. Returns non-zero value in case of error. */
int xdag_add_block(struct xdag_block *b)
{
	pthread_mutex_lock(&block_mutex);
	int res = add_block_nolock(b, time_limit);
	pthread_mutex_unlock(&block_mutex);

	return res;
}

#define setfld(fldtype, src, hashtype) ( \
		b[0].field[0].type |= (uint64_t)(fldtype) << (i << 2), \
			memcpy(&b[0].field[i++], (void*)(src), sizeof(hashtype)) \
		)

#define pretop_block() (top_main_chain && MAIN_TIME(top_main_chain->time) == MAIN_TIME(send_time) ? pretop_main_chain : top_main_chain)

/* create and publish a block
 * The first 'ninput' field 'fields' contains the addresses of the inputs and the corresponding quantity of XDAG,
 * in the following 'noutput' fields similarly - outputs, fee; send_time (time of sending the block);
 * if it is greater than the current one, then the mining is performed to generate the most optimal hash
 */
int xdag_create_block(struct xdag_field *fields, int ninput, int noutput, xdag_amount_t fee, xdag_time_t send_time)
{
	struct xdag_block b[2];
	int i, j, res, res0, mining, defkeynum, keysnum[XDAG_BLOCK_FIELDS], nkeys, nkeysnum = 0, outsigkeyind = -1;
	struct xdag_public_key *defkey = xdag_wallet_default_key(&defkeynum), *keys = xdag_wallet_our_keys(&nkeys), *key;
	xdag_hash_t hash, min_hash;
	struct block_internal *ref, *pretop = pretop_block();

	for (i = 0; i < ninput; ++i) {
		ref = block_by_hash(fields[i].hash);
		if (!ref || !(ref->flags & BI_OURS)) {
			return -1;
		}

		for (j = 0; j < nkeysnum && ref->n_our_key != keysnum[j]; ++j);
			
		if (j == nkeysnum) {
			if (outsigkeyind < 0 && ref->n_our_key == defkeynum) {
				outsigkeyind = nkeysnum;
			}
			keysnum[nkeysnum++] = ref->n_our_key;
		}
	}
	
	res0 = 1 + ninput + noutput + 3 * nkeysnum + (outsigkeyind < 0 ? 2 : 0);
	
	if (res0 > XDAG_BLOCK_FIELDS) {
		return -1;
	}
	
	if (!send_time) {
		send_time = get_timestamp(), mining = 0;
	} else {
		mining = (send_time > get_timestamp() && res0 + 1 <= XDAG_BLOCK_FIELDS);
	}

	res0 += mining;

 begin:
	res = res0;
	memset(b, 0, sizeof(struct xdag_block)); i = 1;
	b[0].field[0].type = XDAG_FIELD_HEAD | (mining ? (uint64_t)XDAG_FIELD_SIGN_IN << ((XDAG_BLOCK_FIELDS - 1) * 4) : 0);
	b[0].field[0].time = send_time;
	b[0].field[0].amount = fee;
	
	if (g_light_mode) {
		if (res < XDAG_BLOCK_FIELDS && ourfirst) {
			setfld(XDAG_FIELD_OUT, ourfirst->hash, xdag_hashlow_t); res++;
		}
	} else {
		if (res < XDAG_BLOCK_FIELDS && mining && pretop && pretop->time < send_time) {
			log_block("Mintop", pretop->hash, pretop->time, pretop->storage_pos);
			setfld(XDAG_FIELD_OUT, pretop->hash, xdag_hashlow_t); res++;
		}

		for (ref = noref_first; ref && res < XDAG_BLOCK_FIELDS; ref = ref->ref) {
			if (ref->time < send_time) {
				setfld(XDAG_FIELD_OUT, ref->hash, xdag_hashlow_t); res++;
			}
		}
	}

	for (j = 0; j < ninput; ++j) {
		setfld(XDAG_FIELD_IN, fields + j, xdag_hash_t);
	}

	for (j = 0; j < noutput; ++j) {
		setfld(XDAG_FIELD_OUT, fields + ninput + j, xdag_hash_t);
	}

	for (j = 0; j < nkeysnum; ++j) {
		key = keys + keysnum[j];
		b[0].field[0].type |= (uint64_t)((j == outsigkeyind ? XDAG_FIELD_SIGN_OUT : XDAG_FIELD_SIGN_IN) * 0x11) << ((i + j + nkeysnum) * 4);
		setfld(XDAG_FIELD_PUBLIC_KEY_0 + ((uintptr_t)key->pub & 1), (uintptr_t)key->pub & ~1l, xdag_hash_t);
	}
	
	if (outsigkeyind < 0) b[0].field[0].type |= (uint64_t)(XDAG_FIELD_SIGN_OUT * 0x11) << ((i + j + nkeysnum) * 4);

	for (j = 0; j < nkeysnum; ++j, i += 2) {
		key = keys + keysnum[j];
		hash_for_signature(b, key, hash);
		xdag_sign(key->key, hash, b[0].field[i].data, b[0].field[i + 1].data);
	}
	
	if (outsigkeyind < 0) {
		hash_for_signature(b, defkey, hash);
		xdag_sign(defkey->key, hash, b[0].field[i].data, b[0].field[i + 1].data);
	}
	
	if (mining) {
		int ntask = g_xdag_pool_task_index + 1;
		struct xdag_pool_task *task = &g_xdag_pool_task[ntask & 1];
		
		xdag_generate_random_array(b[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(xdag_hash_t));
		
		task->task_time = MAIN_TIME(send_time);
		
		xdag_hash_init(task->ctx0);
		xdag_hash_update(task->ctx0, b, sizeof(struct xdag_block) - 2 * sizeof(struct xdag_field));
		xdag_hash_get_state(task->ctx0, task->task[0].data);
		xdag_hash_update(task->ctx0, b[0].field[XDAG_BLOCK_FIELDS - 2].data, sizeof(struct xdag_field));
		memcpy(task->ctx, task->ctx0, xdag_hash_ctx_size());
		
		xdag_hash_update(task->ctx, b[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field) - sizeof(uint64_t));
		memcpy(task->task[1].data, b[0].field[XDAG_BLOCK_FIELDS - 2].data, sizeof(struct xdag_field));
		memcpy(task->nonce.data, b[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field));
		memcpy(task->lastfield.data, b[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(struct xdag_field));
		
		xdag_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);
		g_xdag_pool_task_index = ntask;

		while (get_timestamp() <= send_time) {
			sleep(1);
			struct block_internal *pretop_new = pretop_block();
			if (pretop != pretop_new && get_timestamp() < send_time) {
				pretop = pretop_new;
				xdag_info("Mining: start from beginning because of pre-top block changed");
				goto begin;
			}
		}

		pthread_mutex_lock((pthread_mutex_t*)g_ptr_share_mutex);
		memcpy(b[0].field[XDAG_BLOCK_FIELDS - 1].data, task->lastfield.data, sizeof(struct xdag_field));
		pthread_mutex_unlock((pthread_mutex_t*)g_ptr_share_mutex);
	}

	xdag_hash(b, sizeof(struct xdag_block), min_hash);
	b[0].field[0].transport_header = 1;
	
	log_block("Create", min_hash, b[0].field[0].time, 1);
	
	res = xdag_add_block(b);
	if (res > 0) {
		if (mining) {
			memcpy(g_xdag_mined_hashes[MAIN_TIME(send_time) & (XDAG_POOL_N_CONFIRMATIONS - 1)],
				   min_hash, sizeof(xdag_hash_t));
			memcpy(g_xdag_mined_nonce[MAIN_TIME(send_time) & (XDAG_POOL_N_CONFIRMATIONS - 1)],
				   b[0].field[XDAG_BLOCK_FIELDS - 1].data, sizeof(xdag_hash_t));
		}

		xdag_send_new_block(b); res = 0;
	}

	return res;
}


static int request_blocks(xdag_time_t t, xdag_time_t dt)
{
	int i, res = 0;

	if (!g_xdag_sync_on) return -1;

	if (dt <= REQUEST_BLOCKS_MAX_TIME) {
		xdag_time_t t0 = time_limit;

		for (i = 0;
			xdag_info("QueryB: t=%llx dt=%llx", t, dt),
			i < QUERY_RETRIES && (res = xdag_request_blocks(t, t + dt, &t0, add_block_callback)) < 0;
			++i);
			
		if (res <= 0) {
			return -1;
		}
	} else {
		struct xdag_storage_sum lsums[16], rsums[16];
		if (xdag_load_sums(t, t + dt, lsums) <= 0) {
			return -1;
		}
		
		xdag_debug("Local : [%s]", xdag_log_array(lsums, 16 * sizeof(struct xdag_storage_sum)));

		for (i = 0;
			xdag_info("QueryS: t=%llx dt=%llx", t, dt),
			i < QUERY_RETRIES && (res = xdag_request_sums(t, t + dt, rsums)) < 0;
			++i);

		if (res <= 0) {
			return -1;
		}

		dt >>= 4;
		
		xdag_debug("Remote: [%s]", xdag_log_array(rsums, 16 * sizeof(struct xdag_storage_sum)));

		for (i = 0; i < 16; ++i) {
			if (lsums[i].size != rsums[i].size || lsums[i].sum != rsums[i].sum) {
				request_blocks(t + i * dt, dt);
			}
		}
	}

	return 0;
}

/* a long procedure of synchronization */
static void *sync_thread(void *arg)
{
	xdag_time_t t = 0;

	for (;;) {
		xdag_time_t st = get_timestamp();
		if (st - t >= MAIN_CHAIN_PERIOD) {
			t = st;
			request_blocks(0, 1ll << 48);
		}
		sleep(1);
	}

	return 0;
}

static void reset_callback(struct ldus_rbtree *node)
{
	free(node);
}

// main thread which works with block
static void *work_thread(void *arg)
{
	xdag_time_t t = XDAG_ERA, conn_time = 0, sync_time = 0, t0;
	int n_mining_threads = (int)(unsigned)(uintptr_t)arg, sync_thread_running = 0;
	uint64_t nhashes0 = 0, nhashes = 0;
	pthread_t th;

 begin:
	// loading block from the local storage
	g_xdag_state = XDAG_STATE_LOAD;
	xdag_mess("Loading blocks from local storage...");
	xdag_show_state(0);
	xdag_load_blocks(t, get_timestamp(), &t, add_block_callback);

	// waiting for command "run"
	while (!g_xdag_run) {
		g_xdag_state = XDAG_STATE_STOP;
		sleep(1);
	}

	// launching of synchronization thread
	g_xdag_sync_on = 1;
	if (!g_light_mode && !sync_thread_running) {
		xdag_mess("Starting sync thread...");
		if (!pthread_create(&th, 0, sync_thread, 0)) {
			sync_thread_running = 1;
			pthread_detach(th);
		}
	}

	// start mining threads
	xdag_mess("Starting mining threads...");
	xdag_mining_start(n_mining_threads);

	// periodic generation of blocks and determination of the main block
	xdag_mess("Entering main cycle...");

	for (;;) {
		unsigned nblk;
		
		t0 = t;
		t = get_timestamp();
		nhashes0 = nhashes;
		nhashes = g_xdag_extstats.nhashes;

		if (t > t0) {
			g_xdag_extstats.hashrate_s = ((double)(nhashes - nhashes0) * 1024) / (t - t0);
		}

		if (!g_light_mode && (nblk = (unsigned)g_xdag_extstats.nnoref / (XDAG_BLOCK_FIELDS - 5))) {
			nblk = nblk / 61 + (nblk % 61 > (unsigned)rand() % 61);

			while (nblk--) {
				xdag_create_block(0, 0, 0, 0, 0);
			}
		}
		
		pthread_mutex_lock(&block_mutex);
		
		if (g_xdag_state == XDAG_STATE_REST) {
			g_xdag_sync_on = 0;
			pthread_mutex_unlock(&block_mutex);
			xdag_mining_start(g_light_mode ? ~0 : 0);

			while (get_timestamp() - t < MAIN_CHAIN_PERIOD + (3 << 10)) {
				sleep(1);
			}

			pthread_mutex_lock(&block_mutex);

			if (xdag_free_all()) {
				ldus_rbtree_walk_up(root, reset_callback);
			}

			root = 0;
			g_balance = 0;
			top_main_chain = pretop_main_chain = 0;
			ourfirst = ourlast = noref_first = noref_last = 0;
			memset(&g_xdag_stats, 0, sizeof(g_xdag_stats));
			memset(&g_xdag_extstats, 0, sizeof(g_xdag_extstats));
			pthread_mutex_unlock(&block_mutex);
			conn_time = sync_time = 0;

			goto begin;
		} else if (t > (g_xdag_last_received << 10) && t - (g_xdag_last_received << 10) > 3 * MAIN_CHAIN_PERIOD) {
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

		if (!g_light_mode) {
			check_new_main();
		}

		struct block_internal *ours = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		xdag_show_state(ours ? ours->hash : 0);

		while (get_timestamp() - t < 1024) {
			sleep(1);
		}
	}

	return 0;
}

/* start of regular block processing
 * n_mining_threads - the number of threads for mining on the CPU;
 *   for the light node n_mining_threads < 0 and the number of threads is equal to ~n_mining_threads;
 * miner_address = 1 - the address of the miner is explicitly set
 */
int xdag_blocks_start(int n_mining_threads, int miner_address)
{
	pthread_mutexattr_t attr;
	pthread_t th;

	if (g_xdag_testnet) {
		xdag_era = XDAG_TEST_ERA;
	}

	if (n_mining_threads < 0) {
		g_light_mode = 1;
	}

	if (xdag_mem_init(g_light_mode && !miner_address ? 0 : (((get_timestamp() - XDAG_ERA) >> 10) + (uint64_t)365 * 24 * 60 * 60) * 2 * sizeof(struct block_internal))) {
		return -1;
	}

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&block_mutex, &attr);
	int res = pthread_create(&th, 0, work_thread, (void*)(uintptr_t)(unsigned)n_mining_threads);
	if (!res) {
		pthread_detach(th);
	}
	
	return res;
}

/* returns our first block. If there is no blocks yet - the first block is created. */
int xdag_get_our_block(xdag_hash_t hash)
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = ourfirst;
	pthread_mutex_unlock(&block_mutex);
	
	if (!bi) {
		xdag_create_block(0, 0, 0, 0, 0);
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
int xdag_traverse_our_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
						xdag_amount_t amount, xdag_time_t time, int n_our_key))
{
	struct block_internal *bi;
	int res = 0;

	pthread_mutex_lock(&block_mutex);

	for (bi = ourfirst; !res && bi; bi = bi->ournext) {
		res = (*callback)(data, bi->hash, bi->amount, bi->time, bi->n_our_key);
	}

	pthread_mutex_unlock(&block_mutex);

	return res;
}

static int (*g_traverse_callback)(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time);
static void *g_traverse_data;

static void traverse_all_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = (struct block_internal *)node;

	(*g_traverse_callback)(g_traverse_data, bi->hash, bi->amount, bi->time);
}

/* calls callback for each block */
int xdag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
						xdag_amount_t amount, xdag_time_t time))
{
	pthread_mutex_lock(&block_mutex);
	g_traverse_callback = callback;
	g_traverse_data = data;
	ldus_rbtree_walk_right(root, traverse_all_callback);
	pthread_mutex_unlock(&block_mutex);
	return 0;
}

/* returns current balance for specified address or balance for all addresses if hash == 0 */
xdag_amount_t xdag_get_balance(xdag_hash_t hash)
{
	if (!hash) {
		return g_balance;
	}

	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	
	if (!bi) {
		return 0;
	}
	
	return bi->amount;
}

/* sets current balance for the specified address */
extern int xdag_set_balance(xdag_hash_t hash, xdag_amount_t balance)
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

// returns position and time of block by hash
int64_t xdag_get_block_pos(const xdag_hash_t hash, xdag_time_t *t)
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);

	if (!bi) {
		return -1;
	}
	
	*t = bi->time;
	
	return bi->storage_pos;
}

//returns a number of key by hash of block, or -1 if block is not ours
int xdag_get_key(xdag_hash_t hash)
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	
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
	xdag_time_t tl = (*(struct block_internal **)l)->time, tr = (*(struct block_internal **)r)->time;

	return (tl > tr) - (tl < tr);
}

/* prints detailed information about block */
int xdag_print_block_info(xdag_hash_t hash, FILE *out)
{
	struct tm tm;
	char tbuf[64];
	int i, j;

	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	
	if (!bi) {
		return -1;
	}
	
	uint64_t *h = bi->hash;
	time_t t = bi->time >> 10;
	localtime_r(&t, &tm);
	strftime(tbuf, 64, "%Y-%m-%d %H:%M:%S", &tm);
	fprintf(out, "      time: %s.%03d\n", tbuf, (int)((bi->time & 0x3ff) * 1000) >> 10);
	fprintf(out, " timestamp: %llx\n", (unsigned long long)bi->time);
	fprintf(out, "     flags: %x\n", bi->flags & ~BI_OURS);
	fprintf(out, "  file pos: %llx\n", (unsigned long long)bi->storage_pos);
	fprintf(out, "      hash: %016llx%016llx%016llx%016llx\n",
			(unsigned long long)h[3], (unsigned long long)h[2], (unsigned long long)h[1], (unsigned long long)h[0]);
	fprintf(out, "difficulty: %llx%016llx\n", xdag_diff_args(bi->difficulty));
	fprintf(out, "   balance: %s  %10u.%09u\n", xdag_hash2address(h), pramount(bi->amount));
	fprintf(out, "-------------------------------------------------------------------------------------------\n");
	fprintf(out, "                               block as transaction: details\n");
	fprintf(out, " direction  address                                    amount\n");
	fprintf(out, "-------------------------------------------------------------------------------------------\n");
	fprintf(out, "       fee: %s  %10u.%09u\n", (bi->ref ? xdag_hash2address(bi->ref->hash) : "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"),
			pramount(bi->fee));

	for (i = 0; i < bi->nlinks; ++i) {
		fprintf(out, "    %6s: %s  %10u.%09u\n", (1 << i & bi->in_mask ? " input" : "output"),
				xdag_hash2address(bi->link[i]->hash), pramount(bi->linkamount[i]));
	}
	
	fprintf(out, "-------------------------------------------------------------------------------------------\n");
	fprintf(out, "                                 block as address: details\n");
	fprintf(out, " direction  transaction                                amount       time                   \n");
	fprintf(out, "-------------------------------------------------------------------------------------------\n");
	
	if (bi->flags & BI_MAIN) {
		fprintf(out, "   earning: %s  %10u.%09u  %s.%03d\n", xdag_hash2address(h),
				pramount(MAIN_START_AMOUNT >> ((MAIN_TIME(bi->time) - MAIN_TIME(XDAG_ERA)) >> MAIN_BIG_PERIOD_LOG)),
				tbuf, (int)((bi->time & 0x3ff) * 1000) >> 10);
	}
	
	int N = 0x10000; 
	int n = 0;
	struct block_internal **ba = malloc(N * sizeof(struct block_internal *));
	
	if (!ba) return -1;

	for (struct block_backrefs *br = bi->backrefs; br; br = br->next) {
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

	if (!n) {
		free(ba);
		return 0;
	}

	qsort(ba, n, sizeof(struct block_internal *), bi_compar);

	for (i = 0; i < n; ++i) {
		if (!i || ba[i] != ba[i - 1]) {
			struct block_internal *ri = ba[i];
			if (ri->flags & BI_APPLIED) {
				for (j = 0; j < ri->nlinks; j++) {
					if (ri->link[j] == bi && ri->linkamount[j]) {
						t = ri->time >> 10;
						localtime_r(&t, &tm);
						strftime(tbuf, 64, "%Y-%m-%d %H:%M:%S", &tm);
						fprintf(out, "    %6s: %s  %10u.%09u  %s.%03d\n",
								(1 << j & ri->in_mask ? "output" : " input"), xdag_hash2address(ri->hash),
								pramount(ri->linkamount[j]), tbuf, (int)((ri->time & 0x3ff) * 1000) >> 10);
					}
				}
			}
		}
	}
	
	free(ba);
	
	return 0;
}

// retrieves addresses of N last main blocks
int xdagGetLastMainBlocks(int count, char** addressArray)
{
	int i = 0;
	for (struct block_internal *b = top_main_chain; b && i < count; b = b->link[b->max_diff_link]) {
		if (b->flags & BI_MAIN) {
			strcpy(addressArray[i], xdag_hash2address(b->hash));
			++i;
		}
	}
	return i;
}