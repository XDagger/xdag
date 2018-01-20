/* работа с блоками, T13.654-T13.837 $DVS:time$ */

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
#include "main.h"
#include "sync.h"
#include "pool.h"
#include "memory.h"

#define MAIN_CHAIN_PERIOD	(64 << 10)
#define MAX_WAITING_MAIN	2
#define DEF_TIME_LIMIT		0 /*(MAIN_CHAIN_PERIOD / 2)*/
#define CHEATCOIN_TEST_ERA	0x16900000000ll
#define CHEATCOIN_MAIN_ERA	0x16940000000ll
#define CHEATCOIN_ERA		cheatcoin_era
#define MAIN_START_AMOUNT	(1ll << 42)
#define MAIN_BIG_PERIOD_LOG	21
#define MAIN_TIME(t)		((t) >> 16)
#define MAX_LINKS			15
#define MAKE_BLOCK_PERIOD	13
#define QUERY_RETRIES		2

enum bi_flags {
	BI_MAIN			= 0x01,
	BI_MAIN_CHAIN	= 0x02,
	BI_APPLIED		= 0x04,
	BI_MAIN_REF		= 0x08,
	BI_REF			= 0x10,
	BI_OURS			= 0x20,
};

struct block_internal {
	struct ldus_rbtree node;
	cheatcoin_hash_t hash;
	cheatcoin_diff_t difficulty;
	cheatcoin_amount_t amount, linkamount[MAX_LINKS], fee;
	cheatcoin_time_t time;
	uint64_t storage_pos;
	struct block_internal *ref, *link[MAX_LINKS];
	uint8_t flags, nlinks, max_diff_link, reserved;
	uint16_t in_mask;
	uint16_t n_our_key;
};

#define ourprev link[MAX_LINKS - 2]
#define ournext link[MAX_LINKS - 1]

static cheatcoin_amount_t g_balance = 0;
static cheatcoin_time_t time_limit = DEF_TIME_LIMIT, cheatcoin_era = CHEATCOIN_MAIN_ERA;
static struct ldus_rbtree *root = 0;
static struct block_internal * volatile top_main_chain = 0, * volatile pretop_main_chain = 0;
static struct block_internal *ourfirst = 0, *ourlast = 0, *noref_first = 0, *noref_last = 0;
static pthread_mutex_t block_mutex;
static int g_light_mode = 0;

static uint64_t get_timestamp(void) {
	struct timeval tp;
	gettimeofday(&tp, 0);
	return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

/* возвращает номер текущего периода времени, период - это 64 секунды */
cheatcoin_time_t cheatcoin_main_time(void) {
	return MAIN_TIME(get_timestamp());
}

cheatcoin_time_t cheatcoin_start_main_time(void) {
	return MAIN_TIME(CHEATCOIN_ERA);
}

static inline int lessthen(struct ldus_rbtree *l, struct ldus_rbtree *r) {
	return memcmp(l + 1, r + 1, 24) < 0;
}

ldus_rbtree_define_prefix(lessthen, static inline, )

static struct block_internal *block_by_hash(const cheatcoin_hashlow_t hash) {
	return (struct block_internal *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
}

static void log_block(const char *mess, cheatcoin_hash_t h, cheatcoin_time_t t, uint64_t pos) {
	cheatcoin_info("%s: %016llx%016llx%016llx%016llx t=%llx pos=%llx", mess,
		((uint64_t*)h)[3], ((uint64_t*)h)[2], ((uint64_t*)h)[1], ((uint64_t*)h)[0], t, pos);
}

static inline void accept_amount(struct block_internal *bi, cheatcoin_amount_t sum) {
	if (!sum) return;
	bi->amount += sum;
	if (bi->flags & BI_OURS) {
		struct block_internal *ti;
		g_balance += sum;
		while ((ti = bi->ourprev) && bi->amount > ti->amount) {
			bi->ourprev = ti->ourprev, ti->ournext = bi->ournext, bi->ournext = ti, ti->ourprev = bi;
			*(bi->ourprev ? &bi->ourprev->ournext : &ourfirst) = bi;
			*(ti->ournext ? &ti->ournext->ourprev : &ourlast ) = ti;
		}
		while ((ti = bi->ournext) && bi->amount < ti->amount) {
			bi->ournext = ti->ournext, ti->ourprev = bi->ourprev, bi->ourprev = ti, ti->ournext = bi;
			*(bi->ournext ? &bi->ournext->ourprev : &ourlast ) = bi;
			*(ti->ourprev ? &ti->ourprev->ournext : &ourfirst) = ti;
		}
	}
}

static uint64_t apply_block(struct block_internal *bi) {
	cheatcoin_amount_t sum_in, sum_out;
	int i;
	if (bi->flags & BI_MAIN_REF) return -1l;
	bi->flags |= BI_MAIN_REF;
	for (i = 0; i < bi->nlinks; ++i) {
		cheatcoin_amount_t ref_amount = apply_block(bi->link[i]);
		if (ref_amount == -1l) continue;
		bi->link[i]->ref = bi;
		if (bi->amount + ref_amount >= bi->amount)
			accept_amount(bi, ref_amount);
	}
	sum_in = 0, sum_out = bi->fee;
	for (i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) {
			if (bi->link[i]->amount < bi->linkamount[i]) return 0;
			if (sum_in + bi->linkamount[i] < sum_in) return 0;
			sum_in += bi->linkamount[i];
		} else {
			if (sum_out + bi->linkamount[i] < sum_out) return 0;
			sum_out += bi->linkamount[i];
		}
	}
	if (sum_in + bi->amount < sum_in || sum_in + bi->amount < sum_out) return 0;
	for (i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) accept_amount(bi->link[i], (cheatcoin_amount_t)0 - bi->linkamount[i]);
		else accept_amount(bi->link[i], bi->linkamount[i]);
	}
	accept_amount(bi, sum_in - sum_out);
	bi->flags |= BI_APPLIED;
	return bi->fee;
}

static uint64_t unapply_block(struct block_internal *bi) {
	int i;
	if (bi->flags & BI_APPLIED) {
		cheatcoin_amount_t sum = bi->fee;
		for (i = 0; i < bi->nlinks; ++i) {
			if (1 << i & bi->in_mask)
				accept_amount(bi->link[i],  bi->linkamount[i]), sum -= bi->linkamount[i];
			else
				accept_amount(bi->link[i], (cheatcoin_amount_t)0 - bi->linkamount[i]), sum += bi->linkamount[i];
		}
		accept_amount(bi, sum);
		bi->flags &= ~BI_APPLIED;
	}
	bi->flags &= ~BI_MAIN_REF;
	for (i = 0; i < bi->nlinks; ++i)
		if (bi->link[i]->ref == bi && bi->link[i]->flags & BI_MAIN_REF) accept_amount(bi, unapply_block(bi->link[i]));
	return (cheatcoin_amount_t)0 - bi->fee;
}

/* по данному кол-ву главных блоков возвращает объем циркулирующих читкоинов */
cheatcoin_amount_t cheatcoin_get_supply(uint64_t nmain) {
	cheatcoin_amount_t res = 0, amount = MAIN_START_AMOUNT;
	while (nmain >> MAIN_BIG_PERIOD_LOG) {
		res += (1l << MAIN_BIG_PERIOD_LOG) * amount;
		nmain -= 1l << MAIN_BIG_PERIOD_LOG;
		amount >>= 1;
	}
	res += nmain * amount;
	return res;
}

static void set_main(struct block_internal *m) {
	cheatcoin_amount_t amount = MAIN_START_AMOUNT >> (g_cheatcoin_stats.nmain >> MAIN_BIG_PERIOD_LOG);
	m->flags |= BI_MAIN;
	accept_amount(m, amount);
	g_cheatcoin_stats.nmain++;
	if (g_cheatcoin_stats.nmain > g_cheatcoin_stats.total_nmain)
		g_cheatcoin_stats.total_nmain = g_cheatcoin_stats.nmain;
	accept_amount(m, apply_block(m));
	log_block((m->flags & BI_OURS ? "MAIN +" : "MAIN  "), m->hash, m->time, m->storage_pos);
}

static void unset_main(struct block_internal *m) {
	cheatcoin_amount_t amount;
	g_cheatcoin_stats.nmain--;
	g_cheatcoin_stats.total_nmain--;
	amount = MAIN_START_AMOUNT >> (g_cheatcoin_stats.nmain >> MAIN_BIG_PERIOD_LOG);
	m->flags &= ~BI_MAIN;
	accept_amount(m, (cheatcoin_amount_t)0-amount);
	accept_amount(m, unapply_block(m));
	log_block("UNMAIN", m->hash, m->time, m->storage_pos);
}

static void check_new_main(void) {
	struct block_internal *b, *p = 0;
	int i;
	for (b = top_main_chain, i = 0; b && !(b->flags & BI_MAIN); b = b->link[b->max_diff_link])
		if (b->flags & BI_MAIN_CHAIN) p = b, ++i;
	if (p && i > MAX_WAITING_MAIN) set_main(p);
}

static void unwind_main(struct block_internal *b) {
	struct block_internal *t;
	for (t = top_main_chain; t != b; t = t->link[t->max_diff_link]) 
		{ t->flags &= ~BI_MAIN_CHAIN; if (t->flags & BI_MAIN) unset_main(t); }
}

static inline void hash_for_signature(struct cheatcoin_block b[2], const struct cheatcoin_public_key *key, cheatcoin_hash_t hash) {
	memcpy((uint8_t *)(b + 1) + 1, (void *)((uintptr_t)key->pub & ~1l), sizeof(cheatcoin_hash_t));
	*(uint8_t *)(b + 1) = ((uintptr_t)key->pub & 1) | 0x02;
	cheatcoin_hash(b, sizeof(struct cheatcoin_block) + sizeof(cheatcoin_hash_t) + 1, hash);
	cheatcoin_debug("Hash  : hash=[%s] data=[%s]", cheatcoin_log_hash(hash),
			cheatcoin_log_array(b, sizeof(struct cheatcoin_block) + sizeof(cheatcoin_hash_t) + 1));
}

static inline cheatcoin_diff_t hash_difficulty(cheatcoin_hash_t hash) {
	cheatcoin_diff_t res = ((cheatcoin_diff_t *)hash)[1], max = cheatcoin_diff_max;
	cheatcoin_diff_shr32(&res);
	return cheatcoin_diff_div(max, res);
}

/* возвращает номер открытого ключа из массива keys длины nkeys, который подходит к подписи, начинающейся с поля signo_r блока b,
 * или -1, если ни один не подходит
 */
static int valid_signature(const struct cheatcoin_block *b, int signo_r, int nkeys, struct cheatcoin_public_key *keys) {
	struct cheatcoin_block buf[2];
	cheatcoin_hash_t hash;
	int i, signo_s = -1;
	memcpy(buf, b, sizeof(struct cheatcoin_block));
	for (i = signo_r; i < CHEATCOIN_BLOCK_FIELDS; ++i)
		if (cheatcoin_type(b,i) == CHEATCOIN_FIELD_SIGN_IN || cheatcoin_type(b,i) == CHEATCOIN_FIELD_SIGN_OUT) {
			memset(&buf[0].field[i], 0, sizeof(struct cheatcoin_field));
			if (i > signo_r && signo_s < 0 && cheatcoin_type(b,i) == cheatcoin_type(b,signo_r)) signo_s = i;
		}
	if (signo_s >= 0) for (i = 0; i < nkeys; ++i) {
		hash_for_signature(buf, keys + i, hash);
		if (!cheatcoin_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data)) return i;
	}
	return -1;
}

#define set_pretop(b) if ((b) && MAIN_TIME((b)->time) < MAIN_TIME(timestamp) && \
		(!pretop_main_chain || cheatcoin_diff_gt((b)->difficulty, pretop_main_chain->difficulty))) { \
	pretop_main_chain = (b); \
	log_block("Pretop", (b)->hash, (b)->time, (b)->storage_pos); \
}

/* основная функция; проверить и добавить в базу новый блок; возвращает: > 0 - добавлен, = 0  - уже есть, < 0 - ошибка */
static int add_block_nolock(struct cheatcoin_block *b, cheatcoin_time_t limit) {
	uint64_t timestamp = get_timestamp(), sum_in = 0, sum_out = 0, *psum, theader = b->field[0].transport_header;
	struct cheatcoin_public_key public_keys[16], *our_keys = 0;
	int i, j, k, nkeys = 0, nourkeys = 0, nsignin = 0, nsignout = 0, signinmask = 0, signoutmask = 0, inmask = 0, outmask = 0,
		verified_keys_mask = 0, err, type, nkey;
	struct block_internal bi, *ref, *bsaved, *ref0;
	cheatcoin_diff_t diff0, diff;
	memset(&bi, 0, sizeof(struct block_internal));
	b->field[0].transport_header = 0;
	cheatcoin_hash(b, sizeof(struct cheatcoin_block), bi.hash);
	if (block_by_hash(bi.hash)) return 0;
	if (cheatcoin_type(b,0) != CHEATCOIN_FIELD_HEAD) { i = cheatcoin_type(b,0); err = 1; goto end; }
	bi.time = b->field[0].time;
	if (bi.time > timestamp + MAIN_CHAIN_PERIOD / 4 || bi.time < CHEATCOIN_ERA
			|| (limit && timestamp - bi.time > limit)) { i = 0; err = 2; goto end; }
	if (!g_light_mode) check_new_main();
	for (i = 1; i < CHEATCOIN_BLOCK_FIELDS; ++i) switch((type = cheatcoin_type(b,i))) {
		case CHEATCOIN_FIELD_NONCE:			break;
		case CHEATCOIN_FIELD_IN:			inmask  |= 1 << i; break;
		case CHEATCOIN_FIELD_OUT:			outmask |= 1 << i; break;
		case CHEATCOIN_FIELD_SIGN_IN:		if (++nsignin  & 1) signinmask  |= 1 << i; break;
		case CHEATCOIN_FIELD_SIGN_OUT:		if (++nsignout & 1) signoutmask |= 1 << i; break;
		case CHEATCOIN_FIELD_PUBLIC_KEY_0:
		case CHEATCOIN_FIELD_PUBLIC_KEY_1:
			if ((public_keys[nkeys].key = cheatcoin_public_to_key(b->field[i].data, type - CHEATCOIN_FIELD_PUBLIC_KEY_0)))
				public_keys[nkeys++].pub = (uint64_t *)((uintptr_t)&b->field[i].data | (type - CHEATCOIN_FIELD_PUBLIC_KEY_0));
			break;
		default: err = 3; goto end;
	}
	if (g_light_mode) outmask = 0;
	if (nsignout & 1) { i = nsignout; err = 4; goto end; }
	if (nsignout) our_keys = cheatcoin_wallet_our_keys(&nourkeys);
	for (i = 1; i < CHEATCOIN_BLOCK_FIELDS; ++i) if (1 << i & (signinmask | signoutmask)) {
		nkey = valid_signature(b, i, nkeys, public_keys);
		if (nkey >= 0) verified_keys_mask |= 1 << nkey;
		if (1 << i & signoutmask && !(bi.flags & BI_OURS) && (nkey = valid_signature(b, i, nourkeys, our_keys)) >= 0)
			bi.flags |= BI_OURS, bi.n_our_key = nkey;
	}
	for (i = j = 0; i < nkeys; ++i) if (1 << i & verified_keys_mask) {
		if (i != j) cheatcoin_free_key(public_keys[j].key);
		memcpy(public_keys + j++, public_keys + i, sizeof(struct cheatcoin_public_key));
	}
	nkeys = j;
	bi.difficulty = diff0 = hash_difficulty(bi.hash);
	sum_out += b->field[0].amount;
	bi.fee = b->field[0].amount;
	for (i = 1; i < CHEATCOIN_BLOCK_FIELDS; ++i) if (1 << i & (inmask | outmask)) {
		if (!(ref = block_by_hash(b->field[i].hash))) { err = 5; goto end; }
		if (ref->time >= bi.time) { err = 6; goto end; }
		if (bi.nlinks >= MAX_LINKS) { err = 7; goto end; }
		if (1 << i & inmask) {
			if (b->field[i].amount) {
				struct cheatcoin_block buf, *bref = cheatcoin_storage_load(ref->hash, ref->time, ref->storage_pos, &buf);
				if (!bref) { err = 8; goto end; }
				for (j = k = 0; j < CHEATCOIN_BLOCK_FIELDS; ++j)
					if (cheatcoin_type(bref, j) == CHEATCOIN_FIELD_SIGN_OUT && (++k & 1)
							&& valid_signature(bref, j, nkeys, public_keys) >= 0) break;
				if (j == CHEATCOIN_BLOCK_FIELDS) { err = 9; goto end; }
			}
			psum = &sum_in;
			bi.in_mask |= 1 << bi.nlinks;
		} else psum = &sum_out;
		if (*psum + b->field[i].amount < *psum) { err = 0xA; goto end; }
		*psum += b->field[i].amount;
		bi.link[bi.nlinks] = ref;
		bi.linkamount[bi.nlinks] = b->field[i].amount;
		if (MAIN_TIME(ref->time) < MAIN_TIME(bi.time)) diff = cheatcoin_diff_add(diff0, ref->difficulty);
		else {
			diff = ref->difficulty;
			while (ref && MAIN_TIME(ref->time) == MAIN_TIME(bi.time)) ref = ref->link[ref->max_diff_link];
			if (ref && cheatcoin_diff_gt(cheatcoin_diff_add(diff0, ref->difficulty), diff)) diff = cheatcoin_diff_add(diff0, ref->difficulty);
		}
		if (cheatcoin_diff_gt(diff, bi.difficulty)) bi.difficulty = diff, bi.max_diff_link = bi.nlinks;
		bi.nlinks++;
	}
	if (bi.in_mask ? sum_in < sum_out : sum_out != b->field[0].amount) { err = 0xB; goto end; }
	bsaved = xdag_malloc(sizeof(struct block_internal));
	if (!bsaved) { err = 0xC; goto end; }
	if (!(theader & (sizeof(struct cheatcoin_block) - 1))) bi.storage_pos = theader;
	else bi.storage_pos = cheatcoin_storage_save(b);
	memcpy(bsaved, &bi, sizeof(struct block_internal));
	ldus_rbtree_insert(&root, &bsaved->node);
	g_cheatcoin_stats.nblocks++;
	if (g_cheatcoin_stats.nblocks > g_cheatcoin_stats.total_nblocks)
		g_cheatcoin_stats.total_nblocks = g_cheatcoin_stats.nblocks;
	set_pretop(bsaved);
	set_pretop(top_main_chain);
	if (cheatcoin_diff_gt(bi.difficulty, g_cheatcoin_stats.difficulty)) {
		cheatcoin_info("Diff  : %llx%016llx (+%llx%016llx)", cheatcoin_diff_args(bi.difficulty), cheatcoin_diff_args(diff0));
		for (ref = bsaved, ref0 = 0; ref && !(ref->flags & BI_MAIN_CHAIN); ref = ref->link[ref->max_diff_link]) {
			if ((!ref->link[ref->max_diff_link] || cheatcoin_diff_gt(ref->difficulty, ref->link[ref->max_diff_link]->difficulty))
					&& (!ref0 || MAIN_TIME(ref0->time) > MAIN_TIME(ref->time)))
				{ ref->flags |= BI_MAIN_CHAIN; ref0 = ref; }
		}
		if (ref && MAIN_TIME(ref->time) == MAIN_TIME(bsaved->time)) ref = ref->link[ref->max_diff_link];
		unwind_main(ref);
		top_main_chain = bsaved;
		g_cheatcoin_stats.difficulty = bi.difficulty;
		if (cheatcoin_diff_gt(g_cheatcoin_stats.difficulty, g_cheatcoin_stats.max_difficulty))
			g_cheatcoin_stats.max_difficulty = g_cheatcoin_stats.difficulty;
	}
	if (bi.flags & BI_OURS)
		bsaved->ourprev = ourlast, *(ourlast ? &ourlast->ournext : &ourfirst) = bsaved, ourlast = bsaved;
	for (i = 0; i < bi.nlinks; ++i) if (!(bi.link[i]->flags & BI_REF)) {
		for (ref0 = 0, ref = noref_first; ref != bi.link[i]; ref0 = ref, ref = ref->ref);
		*(ref0 ? &ref0->ref : &noref_first) = ref->ref;
		if (ref == noref_last) noref_last = ref0;
		bi.link[i]->flags |= BI_REF;
		g_cheatcoin_extstats.nnoref--;
	}
	*(noref_last ? &noref_last->ref : &noref_first) = bsaved;
	noref_last = bsaved;
	g_cheatcoin_extstats.nnoref++;
	log_block((bi.flags & BI_OURS ? "Good +" : "Good  "), bi.hash, bi.time, bi.storage_pos);
	i = MAIN_TIME(bsaved->time) & (HASHRATE_LAST_MAX_TIME - 1);
	if (MAIN_TIME(bsaved->time) > MAIN_TIME(g_cheatcoin_extstats.hashrate_last_time)) {
		memset(g_cheatcoin_extstats.hashrate_total + i, 0, sizeof(cheatcoin_diff_t));
		memset(g_cheatcoin_extstats.hashrate_ours  + i, 0, sizeof(cheatcoin_diff_t));
		g_cheatcoin_extstats.hashrate_last_time = bsaved->time;
	}
	if (cheatcoin_diff_gt(diff0, g_cheatcoin_extstats.hashrate_total[i]))
		g_cheatcoin_extstats.hashrate_total[i] = diff0;
	if (bi.flags & BI_OURS && cheatcoin_diff_gt(diff0, g_cheatcoin_extstats.hashrate_ours[i]))
		g_cheatcoin_extstats.hashrate_ours[i] = diff0;
	err = -1;
end:
	for (j = 0; j < nkeys; ++j) cheatcoin_free_key(public_keys[j].key);
	if (err > 0) {
		char buf[32];
		err |= i << 4;
		sprintf(buf, "Err %2x", err & 0xff);
		log_block(buf, bi.hash, bi.time, theader);
	}
	return -err;
}

static void *add_block_callback(void *block, void *data) {
	struct cheatcoin_block *b = (struct cheatcoin_block *)block;
	cheatcoin_time_t *t = (cheatcoin_time_t *)data;
	int res;
	pthread_mutex_lock(&block_mutex);
	if (*t < CHEATCOIN_ERA) (res = add_block_nolock(b, *t));
	else if ((res = add_block_nolock(b, 0)) >= 0 && b->field[0].time > *t) *t = b->field[0].time;
	pthread_mutex_unlock(&block_mutex);
	if (res >= 0) cheatcoin_sync_pop_block(b);
	return 0;
}

/* проверить блок и включить его в базу данных, возвращает не 0 в случае ошибки */
int cheatcoin_add_block(struct cheatcoin_block *b) {
	int res;
	pthread_mutex_lock(&block_mutex);
	res = add_block_nolock(b, time_limit);
	pthread_mutex_unlock(&block_mutex);
	return res;
}

#define setfld(fldtype, src, hashtype) (\
	b[0].field[0].type |= (uint64_t)(fldtype) << (i << 2), \
	memcpy(&b[0].field[i++], (void *)(src), sizeof(hashtype)) \
)

#define pretop_block() (top_main_chain && MAIN_TIME(top_main_chain->time) == MAIN_TIME(send_time) ? pretop_main_chain : top_main_chain)

/* создать и опубликовать блок; в первых ninput полях fields содержатся адреса входов и соотв. кол-во читкоинов,
 * в следующих noutput полях - аналогично - выходы; fee - комиссия; send_time - время отправки блока; если оно больше текущего, то
 * проводится майнинг для генерации наиболее оптимального хеша */
int cheatcoin_create_block(struct cheatcoin_field *fields, int ninput, int noutput, cheatcoin_amount_t fee, cheatcoin_time_t send_time) {
	struct cheatcoin_block b[2];
	int i, j, res, res0, mining, defkeynum, keysnum[CHEATCOIN_BLOCK_FIELDS], nkeys, nkeysnum = 0, outsigkeyind = -1;
	struct cheatcoin_public_key *defkey = cheatcoin_wallet_default_key(&defkeynum), *keys = cheatcoin_wallet_our_keys(&nkeys), *key;
	cheatcoin_hash_t hash, min_hash;
	struct block_internal *ref, *pretop = pretop_block(), *pretop_new;
	for (i = 0; i < ninput; ++i) {
		ref = block_by_hash(fields[i].hash);
		if (!ref || !(ref->flags & BI_OURS)) return -1;
		for (j = 0; j < nkeysnum && ref->n_our_key != keysnum[j]; ++j);
		if (j == nkeysnum) {
			if (outsigkeyind < 0 && ref->n_our_key == defkeynum) outsigkeyind = nkeysnum;
			keysnum[nkeysnum++] = ref->n_our_key;
		}
	}
	res0 = 1 + ninput + noutput + 3 * nkeysnum + (outsigkeyind < 0 ? 2 : 0);
	if (res0 > CHEATCOIN_BLOCK_FIELDS) return -1;
	if (!send_time) send_time = get_timestamp(), mining = 0;
	else mining = (send_time > get_timestamp() && res0 + 1 <= CHEATCOIN_BLOCK_FIELDS);
	res0 += mining;
begin:
	res = res0;
	memset(b, 0, sizeof(struct cheatcoin_block)); i = 1;
	b[0].field[0].type = CHEATCOIN_FIELD_HEAD | (mining ? (uint64_t)CHEATCOIN_FIELD_SIGN_IN << ((CHEATCOIN_BLOCK_FIELDS - 1) * 4) : 0);
	b[0].field[0].time = send_time;
	b[0].field[0].amount = fee;
	if (g_light_mode) {
		if (res < CHEATCOIN_BLOCK_FIELDS && ourfirst) {
			setfld(CHEATCOIN_FIELD_OUT, ourfirst->hash, cheatcoin_hashlow_t); res++;
		}
	} else {
		if (res < CHEATCOIN_BLOCK_FIELDS && mining && pretop && pretop->time < send_time) {
			log_block("Mintop", pretop->hash, pretop->time, pretop->storage_pos);
			setfld(CHEATCOIN_FIELD_OUT, pretop->hash, cheatcoin_hashlow_t); res++;
		}
		for (ref = noref_first; ref && res < CHEATCOIN_BLOCK_FIELDS; ref = ref->ref) if (ref->time < send_time) {
			setfld(CHEATCOIN_FIELD_OUT, ref->hash, cheatcoin_hashlow_t); res++;
		}
	}
	for (j = 0; j < ninput; ++j) setfld(CHEATCOIN_FIELD_IN, fields + j, cheatcoin_hash_t);
	for (j = 0; j < noutput; ++j) setfld(CHEATCOIN_FIELD_OUT, fields + ninput + j, cheatcoin_hash_t);
	for (j = 0; j < nkeysnum; ++j) {
		key = keys + keysnum[j];
		b[0].field[0].type |= (uint64_t)((j == outsigkeyind ? CHEATCOIN_FIELD_SIGN_OUT : CHEATCOIN_FIELD_SIGN_IN) * 0x11) << ((i + j + nkeysnum) * 4);
		setfld(CHEATCOIN_FIELD_PUBLIC_KEY_0 + ((uintptr_t)key->pub & 1), (uintptr_t)key->pub & ~1l, cheatcoin_hash_t);
	}
	if (outsigkeyind < 0) b[0].field[0].type |= (uint64_t)(CHEATCOIN_FIELD_SIGN_OUT * 0x11) << ((i + j + nkeysnum) * 4);
	for (j = 0; j < nkeysnum; ++j, i += 2) {
		key = keys + keysnum[j];
		hash_for_signature(b, key, hash);
		cheatcoin_sign(key->key, hash, b[0].field[i].data, b[0].field[i + 1].data);
	}
	if (outsigkeyind < 0) {
		hash_for_signature(b, defkey, hash);
		cheatcoin_sign(defkey->key, hash, b[0].field[i].data, b[0].field[i + 1].data);
	}
	if (mining) {
		int ntask = g_cheatcoin_pool_ntask + 1;
		struct cheatcoin_pool_task *task = &g_cheatcoin_pool_task[ntask & 1];
		cheatcoin_generate_random_array(b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, sizeof(cheatcoin_hash_t));
		task->main_time = MAIN_TIME(send_time);
		cheatcoin_hash_init(task->ctx0);
		cheatcoin_hash_update(task->ctx0, b, sizeof(struct cheatcoin_block) - 2 * sizeof(struct cheatcoin_field));
		cheatcoin_hash_get_state(task->ctx0, task->task[0].data);
		cheatcoin_hash_update(task->ctx0, b[0].field[CHEATCOIN_BLOCK_FIELDS - 2].data, sizeof(struct cheatcoin_field));
		memcpy(task->ctx, task->ctx0, cheatcoin_hash_ctx_size());
		cheatcoin_hash_update(task->ctx, b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, sizeof(struct cheatcoin_field) - sizeof(uint64_t));
		memcpy(task->task[1].data, b[0].field[CHEATCOIN_BLOCK_FIELDS - 2].data, sizeof(struct cheatcoin_field));
		memcpy(task->nonce.data, b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, sizeof(struct cheatcoin_field));
		memcpy(task->lastfield.data, b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, sizeof(struct cheatcoin_field));
		cheatcoin_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);
		g_cheatcoin_pool_ntask = ntask;
		while (get_timestamp() <= send_time) {
			sleep(1);
			pretop_new = pretop_block();
			if (pretop != pretop_new && get_timestamp() < send_time) {
				pretop = pretop_new;
				cheatcoin_info("Mining: start from beginning because of pre-top block changed");
				goto begin;
			}
		}
		pthread_mutex_lock((pthread_mutex_t *)g_ptr_share_mutex);
		memcpy(b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, task->lastfield.data, sizeof(struct cheatcoin_field));
		pthread_mutex_unlock((pthread_mutex_t *)g_ptr_share_mutex);
	}
	cheatcoin_hash(b, sizeof(struct cheatcoin_block), min_hash);
	b[0].field[0].transport_header = 1;
	log_block("Create", min_hash, b[0].field[0].time, 1);
	res = cheatcoin_add_block(b);
	if (res > 0) {
		if (mining) {
			memcpy(g_cheatcoin_mined_hashes[MAIN_TIME(send_time) & (CHEATCOIN_POOL_N_CONFIRMATIONS - 1)],
					min_hash, sizeof(cheatcoin_hash_t));
			memcpy(g_cheatcoin_mined_nonce[MAIN_TIME(send_time) & (CHEATCOIN_POOL_N_CONFIRMATIONS - 1)],
					b[0].field[CHEATCOIN_BLOCK_FIELDS - 1].data, sizeof(cheatcoin_hash_t));
		}
		cheatcoin_send_new_block(b); res = 0;
	}
	return res;
}


static int request_blocks(cheatcoin_time_t t, cheatcoin_time_t dt) {
	int i, res;
	if (!g_cheatcoin_sync_on) return -1;
	if (dt <= REQUEST_BLOCKS_MAX_TIME) {
		cheatcoin_time_t t0 = time_limit;
		for (i = 0; cheatcoin_info("QueryB: t=%llx dt=%llx", t, dt),
				i < QUERY_RETRIES && (res = cheatcoin_request_blocks(t, t + dt, &t0, add_block_callback)) < 0; ++i);
		if (res <= 0) return -1;
	} else {
		struct cheatcoin_storage_sum lsums[16], rsums[16];
		if (cheatcoin_load_sums(t, t + dt, lsums) <= 0) return -1;
		cheatcoin_debug("Local : [%s]", cheatcoin_log_array(lsums, 16 * sizeof(struct cheatcoin_storage_sum)));
		for (i = 0; cheatcoin_info("QueryS: t=%llx dt=%llx", t, dt),
				i < QUERY_RETRIES && (res = cheatcoin_request_sums(t, t + dt, rsums)) < 0; ++i);
		if (res <= 0) return -1;
		dt >>= 4;
		cheatcoin_debug("Remote: [%s]", cheatcoin_log_array(rsums, 16 * sizeof(struct cheatcoin_storage_sum)));
		for (i = 0; i < 16; ++i) if (lsums[i].size != rsums[i].size || lsums[i].sum != rsums[i].sum)
			request_blocks(t + i * dt, dt);
	}
	return 0;
}

/* длинная процедура синхронизации */
static void *sync_thread(void *arg) {
	cheatcoin_time_t t = 0, st;
	for (;;) {
		st = get_timestamp();
		if (st - t >= MAIN_CHAIN_PERIOD) t = st, request_blocks(0, 1ll << 48);
		sleep(1);
	}
	return 0;
}

static void reset_callback(struct ldus_rbtree *node) {
	free(node);
}

/* основной поток, работающий с блоками */
static void *work_thread(void *arg) {
	cheatcoin_time_t t = CHEATCOIN_ERA, conn_time = 0, sync_time = 0, t0;
	int n_mining_threads = (int)(unsigned)(uintptr_t)arg, sync_thread_running = 0;
	struct block_internal *ours;
	uint64_t nhashes0 = 0, nhashes = 0;
	pthread_t th;

begin:
	/* загрузка блоков из локального хранилища */
	g_cheatcoin_state = CHEATCOIN_STATE_LOAD;
	cheatcoin_mess("Loading blocks from local storage...");
	cheatcoin_show_state(0);
	cheatcoin_load_blocks(t, get_timestamp(), &t, add_block_callback);

	/* ожидание команды run */
	while (!g_cheatcoin_run) {
		g_cheatcoin_state = CHEATCOIN_STATE_STOP;
		sleep(1);
	}

	/* запуск потока синхронизации */
	g_cheatcoin_sync_on = 1;
	if (!g_light_mode && !sync_thread_running) {
		cheatcoin_mess("Starting sync thread...");
		if (!pthread_create(&th, 0, sync_thread, 0)) {
			sync_thread_running = 1;
			pthread_detach(th);
		}
	}

	/* запуск потоков майнинга */
	cheatcoin_mess("Starting mining threads...");
	cheatcoin_mining_start(n_mining_threads);

	/* периодическая генерация блоков и определение главного блока */
	cheatcoin_mess("Entering main cycle...");
	for (;;) {
		unsigned nblk;
		t0 = t;
		t = get_timestamp();
		nhashes0 = nhashes;
		nhashes = g_cheatcoin_extstats.nhashes;
		if (t > t0) g_cheatcoin_extstats.hashrate_s = ((double)(nhashes - nhashes0) * 1024) / (t - t0);
		if (!g_light_mode && (nblk = (unsigned)g_cheatcoin_extstats.nnoref / (CHEATCOIN_BLOCK_FIELDS - 5))) {
			nblk = nblk / 61 + (nblk % 61 > (unsigned)rand() % 61);
			while (nblk--) cheatcoin_create_block(0, 0, 0, 0, 0);
		}
		pthread_mutex_lock(&block_mutex);
		if (g_cheatcoin_state == CHEATCOIN_STATE_REST) {
			g_cheatcoin_sync_on = 0;
			pthread_mutex_unlock(&block_mutex);
			cheatcoin_mining_start(g_light_mode ? ~0 : 0);
			while (get_timestamp() - t < MAIN_CHAIN_PERIOD + (3 << 10)) sleep(1);
			pthread_mutex_lock(&block_mutex);
			ldus_rbtree_walk_up(root, reset_callback);
			root = 0;
			g_balance = 0;
			top_main_chain = pretop_main_chain = 0;
			ourfirst = ourlast = noref_first = noref_last = 0;
			memset(&g_cheatcoin_stats, 0, sizeof(g_cheatcoin_stats));
			memset(&g_cheatcoin_extstats, 0, sizeof(g_cheatcoin_extstats));
			pthread_mutex_unlock(&block_mutex);
			conn_time = sync_time = 0;
			goto begin;
		} else if (t > (g_cheatcoin_last_received << 10) && t - (g_cheatcoin_last_received << 10) > 3 * MAIN_CHAIN_PERIOD) {
			g_cheatcoin_state = (g_light_mode ? (g_cheatcoin_testnet ? CHEATCOIN_STATE_TTST : CHEATCOIN_STATE_TRYP)
					: (g_cheatcoin_testnet ? CHEATCOIN_STATE_WTST : CHEATCOIN_STATE_WAIT));
			conn_time = sync_time = 0;
		} else {
			if (!conn_time) conn_time = t;
			if (!g_light_mode && t - conn_time >= 2 * MAIN_CHAIN_PERIOD
					&& !memcmp(&g_cheatcoin_stats.difficulty, &g_cheatcoin_stats.max_difficulty, sizeof(cheatcoin_diff_t)))
				sync_time = t;
			if (t - (g_cheatcoin_xfer_last << 10) <= (g_light_mode ? 4 : 3) * MAIN_CHAIN_PERIOD)
				g_cheatcoin_state = CHEATCOIN_STATE_XFER;
			else if (g_light_mode) {
				g_cheatcoin_state = (g_cheatcoin_mining_threads > 0 ?
						  (g_cheatcoin_testnet ? CHEATCOIN_STATE_MTST : CHEATCOIN_STATE_MINE)
						: (g_cheatcoin_testnet ? CHEATCOIN_STATE_PTST : CHEATCOIN_STATE_POOL));
			} else if (t - sync_time > 8 * MAIN_CHAIN_PERIOD)
				g_cheatcoin_state = (g_cheatcoin_testnet ? CHEATCOIN_STATE_CTST : CHEATCOIN_STATE_CONN);
			else
				g_cheatcoin_state = (g_cheatcoin_testnet ? CHEATCOIN_STATE_STST : CHEATCOIN_STATE_SYNC);
		}
		if (!g_light_mode) check_new_main();
		ours = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		cheatcoin_show_state(ours ? ours->hash : 0);
		while (get_timestamp() - t < 1024) sleep(1);
	}
	return 0;
}

/* начало регулярной обработки блоков; n_mining_threads - число потоков для майнинга на CPU;
 * для лёгкой ноды n_mining_threads < 0 и число потоков майнинга равно ~n_mining_threads;
 * miner_address = 1 - явно задан адрес майнера */
int cheatcoin_blocks_start(int n_mining_threads, int miner_address) {
	pthread_mutexattr_t attr;
	pthread_t th;
	int res;
	if (g_cheatcoin_testnet) cheatcoin_era = CHEATCOIN_TEST_ERA;
	if (n_mining_threads < 0) g_light_mode = 1;
	if (xdag_mem_init(g_light_mode && !miner_address ? 0 : (((get_timestamp() - CHEATCOIN_ERA) >> 10) + (uint64_t)365 * 24 * 60 * 60) * 2 * sizeof(struct block_internal)))
		return -1;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&block_mutex, &attr);
	res = pthread_create(&th, 0, work_thread, (void *)(uintptr_t)(unsigned)n_mining_threads);
	if (!res) pthread_detach(th);
	return res;
}

/* выдаёт первый наш блок, а если его нет - создаёт */
int cheatcoin_get_our_block(cheatcoin_hash_t hash) {
	struct block_internal *bi;
	pthread_mutex_lock(&block_mutex);
	bi = ourfirst;
	pthread_mutex_unlock(&block_mutex);
	if (!bi) {
		cheatcoin_create_block(0, 0, 0, 0, 0);
		pthread_mutex_lock(&block_mutex);
		bi = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		if (!bi) return -1;
	}
	memcpy(hash, bi->hash, sizeof(cheatcoin_hash_t));
	return 0;
}

/* для каждого своего блока вызывается callback */
int cheatcoin_traverse_our_blocks(void *data, int (*callback)(void *data, cheatcoin_hash_t hash,
		cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key)) {
	struct block_internal *bi;
	int res = 0;
	pthread_mutex_lock(&block_mutex);
	for (bi = ourfirst; !res && bi; bi = bi->ournext)
		res = (*callback)(data, bi->hash, bi->amount, bi->time, bi->n_our_key);
	pthread_mutex_unlock(&block_mutex);
	return res;
}

static int (*g_traverse_callback)(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time);
static void *g_traverse_data;

static void traverse_all_callback(struct ldus_rbtree *node) {
	struct block_internal *bi = (struct block_internal *)node;
	(*g_traverse_callback)(g_traverse_data, bi->hash, bi->amount, bi->time);
}

/* для каждого блока вызывается callback */
int cheatcoin_traverse_all_blocks(void *data, int (*callback)(void *data, cheatcoin_hash_t hash,
		cheatcoin_amount_t amount, cheatcoin_time_t time)) {
	pthread_mutex_lock(&block_mutex);
	g_traverse_callback = callback;
	g_traverse_data = data;
	ldus_rbtree_walk_right(root, traverse_all_callback);
	pthread_mutex_unlock(&block_mutex);
	return 0;
}

/* возвращает баланс адреса, или всех наших адресов, если hash == 0 */
cheatcoin_amount_t cheatcoin_get_balance(cheatcoin_hash_t hash) {
	struct block_internal *bi;
	if (!hash) return g_balance;
	pthread_mutex_lock(&block_mutex);
	bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	if (!bi) return 0;
	return bi->amount;
}

/* устанавливает баланс адреса */
extern int cheatcoin_set_balance(cheatcoin_hash_t hash, cheatcoin_amount_t balance) {
	struct block_internal *bi;
	if (!hash) return -1;
	pthread_mutex_lock(&block_mutex);
	bi = block_by_hash(hash);
	if (bi->flags & BI_OURS && bi != ourfirst) {
		if (bi->ourprev) bi->ourprev->ournext = bi->ournext; else ourfirst = bi->ournext;
		if (bi->ournext) bi->ournext->ourprev = bi->ourprev; else ourlast = bi->ourprev;
		bi->ourprev = 0;
		bi->ournext = ourfirst;
		if (ourfirst) ourfirst->ourprev = bi; else ourlast = bi;
		ourfirst = bi;
	}
	pthread_mutex_unlock(&block_mutex);
	if (!bi) return -1;
	if (bi->amount != balance) {
		cheatcoin_hash_t hash0;
		cheatcoin_amount_t diff;
		memset(hash0, 0, sizeof(cheatcoin_hash_t));
		if (balance > bi->amount) {
			diff = balance - bi->amount;
			cheatcoin_log_xfer(hash0, hash, diff);
			if (bi->flags & BI_OURS) g_balance += diff;
		} else {
			diff = bi->amount - balance;
			cheatcoin_log_xfer(hash, hash0, diff);
			if (bi->flags & BI_OURS) g_balance -= diff;
		}
		bi->amount = balance;
	}
	return 0;
}

/* по хешу блока возвращает его позицию в хранилище и время */
int64_t cheatcoin_get_block_pos(const cheatcoin_hash_t hash, cheatcoin_time_t *t) {
	struct block_internal *bi;
	pthread_mutex_lock(&block_mutex);
	bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	if (!bi) return -1;
	*t = bi->time;
	return bi->storage_pos;
}

/* по хешу блока возвращает номер ключа или -1, если блок не наш */
int cheatcoin_get_key(cheatcoin_hash_t hash) {
	struct block_internal *bi;
	pthread_mutex_lock(&block_mutex);
	bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);
	if (!bi || !(bi->flags & BI_OURS)) return -1;
	return bi->n_our_key;
}

/* переинициализация системы обработки блоков */
int cheatcoin_blocks_reset(void) {
	pthread_mutex_lock(&block_mutex);
	if (g_cheatcoin_state != CHEATCOIN_STATE_REST) {
		cheatcoin_crit("The local storage is corrupted. Resetting blocks engine.");
		g_cheatcoin_state = CHEATCOIN_STATE_REST;
		cheatcoin_show_state(0);
	}
	pthread_mutex_unlock(&block_mutex);
	return 0;
}
