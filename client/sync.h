/* синхронизация, T13.738-T14.582 $DVS:time$ */

#ifndef XDAG_SYNC_H
#define XDAG_SYNC_H

#include "block.h"
#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sync_block {
    struct xdag_block b;
    xdag_hash_t hash;
    time_t t;
    uint8_t nfield;
    uint8_t ttl;
};

extern int g_xdag_sync_on;
	
/* checks a block and includes it in the database with synchronization, ruturs non-zero value in case of error */
extern int xdag_sync_add_block(struct xdag_block *b, struct xconnection *conn);

/* notifies synchronization mechanism about found block */
extern int xdag_sync_pop_block(struct xdag_block *b);

/* initialized block synchronization */
extern int xdag_sync_init(void);
	
#ifdef __cplusplus
};
#endif

#endif
