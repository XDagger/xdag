/* синхронизация, T13.738-T14.582 $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "sync.h"
#include "hash.h"
#include "global.h"
#include "transport.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "time.h"

#define SYNC_HASH_SIZE      0x10000
#define get_list(hash)      (g_sync_hash   + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define get_list_r(hash)    (g_sync_hash_r + ((hash)[0] & (SYNC_HASH_SIZE - 1)))
#define REQ_PERIOD          64
#define QUERY_RETRIES       2

//static struct sync_block **g_sync_hash, **g_sync_hash_r;
static pthread_mutex_t g_sync_hash_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_xdag_sync_on = 0;
extern xtime_t g_time_limit;

//functions
int xdag_sync_add_block_nolock(struct xdag_block*, struct xconnection*);
int xdag_sync_pop_block_nolock(struct xdag_block*);
extern void *add_block_callback_sync(void *block, void *data);
void *sync_thread(void*);

/* moves the block to the wait list, block with hash written to field 'nfield' of block 'b' is expected 
 (original russian comment was unclear too) */
static int push_block_nolock(struct xdag_block *b, struct xconnection *conn, int nfield, int ttl)
{
	xdag_hashlow_t ref_hash = {0};
	xdag_hash_t block_hash = {0};
	struct sync_block p, q;
	int res = 0;
	time_t t = time(0);

    memcpy(ref_hash, b->field[nfield].hash, sizeof(xdag_hashlow_t));
    xdag_hash(b, sizeof(struct xdag_block), block_hash);

//    for (p = get_list(b->field[nfield].hash), q = *p; q; q = q->next) {
//        if (!memcmp(&q->b, b, sizeof(struct xdag_block))) {
//            res = (t - q->t >= REQ_PERIOD);
//
//            q->conn = conn;
//            q->nfield = nfield;
//            q->ttl = ttl;
//
//            if (res) q->t = t;
//
//            return res;
//        }
//    }

	if(!xdag_rsdb_get_syncblock(ref_hash, block_hash, &p))
	{
		res = (t - p.t >= REQ_PERIOD);
		p.nfield = nfield;
		p.ttl = ttl;
		if (res) p.t = t;
		xdag_rsdb_put_syncblock(ref_hash, block_hash, &p);
		return res;
    }

	memcpy(&q.b, b, sizeof(struct xdag_block));
	q.nfield = nfield;
	q.ttl = ttl;
	q.t = t;
    xdag_rsdb_put_syncblock(ref_hash, block_hash, &q);
    xdag_info("sync push: ref_hash:%016llx%016llx%016llx%016llx, hash:%016llx%016llx%016llx%016llx", ref_hash[3], ref_hash[2], ref_hash[1], ref_hash[0], block_hash[3],block_hash[2],block_hash[1],block_hash[0]);
//	*p = q;
//	p = get_list_r(hash);
//	q->next_r = *p;
//	*p = q;
	g_xdag_extstats.nwaitsync++;
    xdag_rsdb_put_extstats();
	return 1;
}

/* notifies synchronization mechanism about found block */
int xdag_sync_pop_block_nolock(struct xdag_block *b)
{
	struct sync_block p;
	xdag_hash_t hash = {0};
	xdag_hash(b, sizeof(struct xdag_block), hash);
 
//begin:


//	for (p = get_list(hash); (q = *p); p = &q->next) {
//		if (!memcmp(hash, q->b.field[q->nfield].hash, sizeof(xdag_hashlow_t))) {
//			*p = q->next;
//			g_xdag_extstats.nwaitsync--;
//            xdag_rsdb_put_extstats();
//			for (p = get_list_r(q->hash); (r = *p) && r != q; p = &r->next_r);
//
//			if (r == q) {
//				*p = q->next_r;
//			}
//
//			q->b.field[0].transport_header = q->ttl << 8 | 1;
//			xdag_sync_add_block_nolock(&q->b, q->conn);
//			free(q);
//
//			goto begin;
//		}
//	}
    char seek_key[1 + sizeof(xdag_hashlow_t)] = {[0] = HASH_BLOCK_SYNC};
    memcpy(seek_key + 1, hash, sizeof(xdag_hashlow_t));
    rocksdb_iterator_t* iter = rocksdb_create_iterator(g_xdag_rsdb->db, g_xdag_rsdb->read_options);
    size_t vlen = 0;
    for (rocksdb_iter_seek(iter, seek_key, sizeof(seek_key));
         rocksdb_iter_valid(iter) && !memcmp(seek_key, rocksdb_iter_key(iter, &vlen), sizeof(seek_key));
         rocksdb_iter_next(iter)) {
        const char *value = rocksdb_iter_value(iter, &vlen);
        if(value) {
            xdag_hash_t block_hash = {0};
            memcpy(&p, value, sizeof(struct sync_block));
            xdag_hash(&p, sizeof(struct xdag_block), block_hash);
            g_xdag_extstats.nwaitsync--;
            p.b.field[0].transport_header = p.ttl << 8 | 1;
            xdag_rsdb_put_extstats();
            xdag_rsdb_del_syncblock(hash, block_hash);
            xdag_sync_add_block_nolock(&(p.b), NULL);
        }

    }
    if(iter) rocksdb_iter_destroy(iter);

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
int xdag_sync_add_block_nolock(struct xdag_block *b, struct xconnection *conn)
{
	int res=0, ttl = b->field[0].transport_header >> 8 & 0xff;

	res = xdag_add_block(b);
	if (res >= 0) {
		xdag_sync_pop_block_nolock(b);
		if (res > 0 && ttl > 2) {
			b->field[0].transport_header = ttl << 8;
			xdag_send_packet(b, conn, 1);
		}
	} else if (g_xdag_sync_on && ((res = -res) & 0xf) == 5) {
		res = (res >> 4) & 0xf;
		if (push_block_nolock(b, conn, res, ttl)) {
			struct sync_block q;
			// this is not exist block's hash at this xdag_blocks
//			uint64_t *hash = b->field[res].hash;
            xdag_hashlow_t ref_hash = {0};
            xdag_hash_t block_hash = {0};
            memcpy(ref_hash, b->field[res].hash, sizeof(xdag_hashlow_t));
            xdag_hash(b, sizeof(struct xdag_block), block_hash);
			time_t t = time(0);
 
//begin:
//			for (p = get_list_r(hash); (q = *p); p = &q->next_r) {
//                if (!memcmp(hash, q->hash, sizeof(xdag_hashlow_t))) {
//                    if (t - q->t < REQ_PERIOD) {
//                        return 0;
//                    }
//
//                    q->t = t;
//                    hash = q->b.field[q->nfield].hash;
//
//                    goto begin;
//                }
//            }
            char seek_key[1 + sizeof(xdag_hashlow_t)] = {[0] = HASH_BLOCK_SYNC};
            memcpy(seek_key + 1, ref_hash, sizeof(xdag_hashlow_t));
            rocksdb_iterator_t* iter = rocksdb_create_iterator(g_xdag_rsdb->db, g_xdag_rsdb->read_options);
            size_t vlen = 0;
            for (rocksdb_iter_seek(iter, seek_key, sizeof(seek_key));
                 rocksdb_iter_valid(iter) && !memcmp(seek_key, rocksdb_iter_key(iter, &vlen), sizeof(seek_key));
                 rocksdb_iter_next(iter)) {
                const char *value = rocksdb_iter_value(iter, &vlen);
                if(value) {
                    memcpy(&q, value, sizeof(struct sync_block));
                    if (t - q.t < REQ_PERIOD) {
                        continue;
                    }
                    q.t = t;
                    xdag_hash(&q, sizeof(struct xdag_block), block_hash);
                    xdag_rsdb_put_syncblock(ref_hash, block_hash, &q);
                }

            }
            if(iter) rocksdb_iter_destroy(iter);

			xdag_request_block(ref_hash, NULL, 1);
			xdag_info("ReqBlk: %016llx%016llx%016llx%016llx", ref_hash[3], ref_hash[2], ref_hash[1], ref_hash[0]);
		}
	}

	return 0;
}

int xdag_sync_add_block(struct xdag_block *b, struct xconnection *conn)
{
	pthread_mutex_lock(&g_sync_hash_mutex);
	int res = xdag_sync_add_block_nolock(b, conn);
	pthread_mutex_unlock(&g_sync_hash_mutex);
	return res;
}

/* initialized block synchronization */
int xdag_sync_init(void)
{
//	g_sync_hash = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);
//	g_sync_hash_r = (struct sync_block **)calloc(sizeof(struct sync_block *), SYNC_HASH_SIZE);
//
//	if (!g_sync_hash || !g_sync_hash_r) return -1;

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
			i < QUERY_RETRIES && (res = xdag_request_blocks(t, t + dt, &t0, add_block_callback_sync)) < 0;
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
            if(g_xdag_state != XDAG_STATE_LOAD) {
                t = st;
                request_blocks(0, 1ll << 48);
            } else {
                xdag_info("sync_thread wait for Local load data : t=%llx", t);
            }
		}
		sleep(1);
	}

	return 0;
}
