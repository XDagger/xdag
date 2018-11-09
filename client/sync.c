/* синхронизация, T13.738-T14.582 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "sync.h"
#include "hash.h"
#include "init.h"
#include "transport.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "time.h"

#define SYNC_HASH_SIZE      0x10000
#define get_list(hash)      (g_sync_hash   + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define get_list_r(hash)    (g_sync_hash_r + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define REQ_PERIOD          64
#define QUERY_RETRIES       2

struct sync_block {
	struct xdag_block b;
	xdag_hash_t hash;
	struct sync_block *next, *next_r;
	void *conn;
	time_t t;
	uint8_t nfield;
	uint8_t ttl;
};

static struct sync_block **g_sync_hash, **g_sync_hash_r;
static pthread_mutex_t g_sync_hash_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_xdag_sync_on = 0;
extern xtime_t g_time_limit;

//functions
int xdag_sync_add_block_nolock(struct xdag_block*, void*);
int xdag_sync_pop_block_nolock(struct xdag_block*);
extern void *add_block_callback(void *block, void *data);
void *sync_thread(void*);

/* moves the block to the wait list, block with hash written to field 'nfield' of block 'b' is expected 
 (original russian comment was unclear too) */
static int push_block_nolock(struct xdag_block *b, void *conn, int nfield, int ttl)
{
	xdag_hash_t hash;
	struct sync_block **p, *q;
	int res;
	time_t t = time(0);

	xdag_hash(b, sizeof(struct xdag_block), hash);

	for (p = get_list(b->field[nfield].hash), q = *p; q; q = q->next) {
		if (!memcmp(&q->b, b, sizeof(struct xdag_block))) {
			res = (t - q->t >= REQ_PERIOD);
			
			q->conn = conn;
			q->nfield = nfield;
			q->ttl = ttl;
			
			if (res) q->t = t;
			
			return res;
		}
	}

	q = (struct sync_block *)malloc(sizeof(struct sync_block));
	if (!q) return -1;
	
	memcpy(&q->b, b, sizeof(struct xdag_block));
	memcpy(&q->hash, hash, sizeof(xdag_hash_t));
	
	q->conn = conn;
	q->nfield = nfield;
	q->ttl = ttl;
	q->t = t;
	q->next = *p;
	
	*p = q;
	p = get_list_r(hash);
	
	q->next_r = *p;
	*p = q;
	
	g_xdag_extstats.nwaitsync++;
	
	return 1;
}

/* notifies synchronization mechanism about found block */
int xdag_sync_pop_block_nolock(struct xdag_block *b)
{
	struct sync_block **p, *q, *r;
	xdag_hash_t hash;

	xdag_hash(b, sizeof(struct xdag_block), hash);
 
begin:

	for (p = get_list(hash); (q = *p); p = &q->next) {
		if (!memcmp(hash, q->b.field[q->nfield].hash, sizeof(xdag_hashlow_t))) {
			*p = q->next;
			g_xdag_extstats.nwaitsync--;

			for (p = get_list_r(q->hash); (r = *p) && r != q; p = &r->next_r);
				
			if (r == q) {
				*p = q->next_r;
			}
			
			q->b.field[0].transport_header = q->ttl << 8 | 1;
			xdag_sync_add_block_nolock(&q->b, q->conn);			
			free(q);
			
			goto begin;
		}
	}

	return 0;
}

int xdag_sync_pop_block(struct xdag_block *b)
{
	pthread_mutex_lock(&g_sync_hash_mutex);
	int res = xdag_sync_pop_block_nolock(b);
	pthread_mutex_unlock(&g_sync_hash_mutex);
	return res;
}

/* checks a block and includes it in the database with synchronization, ruturs non-zero value in case of error */
int xdag_sync_add_block_nolock(struct xdag_block *b, void *conn)
{
	int res=0, ttl = b->field[0].transport_header >> 8 & 0xff;

	res = xdag_add_block(b);
	if (res >= 0) {
		xdag_sync_pop_block_nolock(b);
		if (res > 0 && ttl > 2) {
			b->field[0].transport_header = ttl << 8;
			xdag_send_packet(b, (void*)((uintptr_t)conn | 1l));
		}
	} else if (g_xdag_sync_on && ((res = -res) & 0xf) == 5) {
		res = (res >> 4) & 0xf;
		if (push_block_nolock(b, conn, res, ttl)) {
			struct sync_block **p, *q;
			uint64_t *hash = b->field[res].hash;
			time_t t = time(0);
 
begin:
			for (p = get_list_r(hash); (q = *p); p = &q->next_r) {
				if (!memcmp(hash, q->hash, sizeof(xdag_hashlow_t))) {
					if (t - q->t < REQ_PERIOD) {
						return 0;
					}

					q->t = t;
					hash = q->b.field[q->nfield].hash;

					goto begin;
				}
			}
			
			xdag_request_block(hash, (void*)(uintptr_t)1l);
			
			xdag_info("ReqBlk: %016llx%016llx%016llx%016llx", hash[3], hash[2], hash[1], hash[0]);
		}
	}

	return 0;
}

int xdag_sync_add_block(struct xdag_block *b, void *conn)
{
	pthread_mutex_lock(&g_sync_hash_mutex);
	int res = xdag_sync_add_block_nolock(b, conn);
	pthread_mutex_unlock(&g_sync_hash_mutex);
	return res;
}

/* initialized block synchronization */
int xdag_sync_init(void)
{
	g_sync_hash = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);
	g_sync_hash_r = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);

	if (!g_sync_hash || !g_sync_hash_r) return -1;

	return 0;
}

// request all blocks between t and t + dt
static int request_blocks(xtime_t t, xtime_t dt)
{
	int i, res = 0;

	if (!g_xdag_sync_on) return -1;

	if (dt <= REQUEST_BLOCKS_MAX_TIME) {
		xtime_t t0 = g_time_limit;

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
void *sync_thread(void *arg)
{
	xtime_t t = 0;

	for (;;) {
		xtime_t st = xdag_get_xtimestamp();
		if (st - t >= MAIN_CHAIN_PERIOD) {
			t = st;
			request_blocks(0, 1ll << 48);
		}
		sleep(1);
	}

	return 0;
}
