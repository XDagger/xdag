/* синхронизация, T13.738-T13.764 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "sync.h"
#include "hash.h"
#include "main.h"
#include "transport.h"
#include "log.h"

#define SYNC_HASH_SIZE		0x10000
#define get_list(hash)		(g_sync_hash   + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define get_list_r(hash)	(g_sync_hash_r + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define REQ_PERIOD			64

struct sync_block {
	struct cheatcoin_block b;
	cheatcoin_hash_t hash;
	struct sync_block *next, *next_r;
	void *conn;
	time_t t;
	uint8_t nfield;
	uint8_t ttl;
};

static struct sync_block **g_sync_hash, **g_sync_hash_r;
static pthread_mutex_t g_sync_hash_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_cheatcoin_sync_on = 0;

/* заносит блок в лист ожидания, ожидается блок с хешем, записанным в поле nfield блока b */
static int push_block(struct cheatcoin_block *b, void *conn, int nfield, int ttl) {
	cheatcoin_hash_t hash;
	struct sync_block **p, *q;
	int res;
	time_t t = time(0);
	cheatcoin_hash(b, sizeof(struct cheatcoin_block), hash);
	pthread_mutex_lock(&g_sync_hash_mutex);
	for (p = get_list(b->field[nfield].hash), q = *p; q; q = q->next) {
		if (!memcmp(&q->b, b, sizeof(struct cheatcoin_block))) {
			res = (t - q->t >= REQ_PERIOD);
			q->conn = conn;
			q->nfield = nfield;
			q->ttl = ttl;
			if (res) q->t = t;
			pthread_mutex_unlock(&g_sync_hash_mutex);
			return res;
		}
	}
	q = (struct sync_block *)malloc(sizeof(struct sync_block));
	if (!q) return -1;
	memcpy(&q->b, b, sizeof(struct cheatcoin_block));
	memcpy(&q->hash, hash, sizeof(cheatcoin_hash_t));
	q->conn = conn;
	q->nfield = nfield;
	q->ttl = ttl;
	q->t = t;
	q->next = *p;
	*p = q;
	p = get_list_r(hash);
	q->next_r = *p;
	*p = q;
	g_cheatcoin_extstats.nwaitsync++;
	pthread_mutex_unlock(&g_sync_hash_mutex);
	return 1;
}

/* извещает механизм синхронизации, что искомый блок уже найден */
int cheatcoin_sync_pop_block(struct cheatcoin_block *b) {
	struct sync_block **p, *q, *r;
	cheatcoin_hash_t hash;
	cheatcoin_hash(b, sizeof(struct cheatcoin_block), hash);
begin:
	pthread_mutex_lock(&g_sync_hash_mutex);
	for (p = get_list(hash); (q = *p); p = &q->next)
		if (!memcmp(hash, q->b.field[q->nfield].hash, sizeof(cheatcoin_hashlow_t))) {
			*p = q->next;
			g_cheatcoin_extstats.nwaitsync--;
			for (p = get_list_r(q->hash); (r = *p) && r != q; p = &r->next_r);
			if (r == q) *p = q->next_r;
			pthread_mutex_unlock(&g_sync_hash_mutex);
			q->b.field[0].transport_header = q->ttl << 8 | 1;
			cheatcoin_sync_add_block(&q->b, q->conn);
			free(q);
			goto begin;
		}
	pthread_mutex_unlock(&g_sync_hash_mutex);
	return 0;
}

/* проверить блок и включить его в базу данных с учётом синхронизации, возвращает не 0 в случае ошибки */
int cheatcoin_sync_add_block(struct cheatcoin_block *b, void *conn) {
	int res, ttl = b->field[0].transport_header >> 8 & 0xff;
	res = cheatcoin_add_block(b);
	if (res >= 0) {
		cheatcoin_sync_pop_block(b);
		if (res > 0 && ttl > 2) {
			b->field[0].transport_header = ttl << 8;
			cheatcoin_send_packet(b, (void *)((uintptr_t)conn | 1l));
		}
	} else if (g_cheatcoin_sync_on && ((res = -res) & 0xf) == 5) {
		res = (res >> 4) & 0xf;
		if (push_block(b, conn, res, ttl)) {
			struct sync_block **p, *q;
			uint64_t *hash = b->field[res].hash;
			time_t t = time(0);
			pthread_mutex_lock(&g_sync_hash_mutex);
		begin:
			for (p = get_list_r(hash); (q = *p); p = &q->next_r)
				if (!memcmp(hash, q->hash, sizeof(cheatcoin_hashlow_t))) {
					if (t - q->t < REQ_PERIOD) {
						pthread_mutex_unlock(&g_sync_hash_mutex);
						return 0;
					}
					q->t = t;
					hash = q->b.field[q->nfield].hash;
					goto begin;
				}
			pthread_mutex_unlock(&g_sync_hash_mutex);
			cheatcoin_request_block(hash, (void *)(uintptr_t)1l);
			cheatcoin_info("ReqBlk: %016llx%016llx%016llx%016llx", hash[3], hash[2], hash[1], hash[0]);
		}
	}
	return 0;
}

/* инициализация синхронизации блоков */
int cheatcoin_sync_init(void) {
	g_sync_hash = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);
	g_sync_hash_r = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);
	if (!g_sync_hash || !g_sync_hash_r) return -1;
	return 0;
}
