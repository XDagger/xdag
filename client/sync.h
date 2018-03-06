/* синхронизация, T13.738-T13.764 $DVS:time$ */

#ifndef XDAG_SYNC_H
#define XDAG_SYNC_H

#include "block.h"

/* checks a block and includes it in the database with synchronization, ruturs non-zero value in case of error */
extern int xdag_sync_add_block(struct xdag_block *b, void *conn);

/* notifies synchronization mechanism about found block */
extern int xdag_sync_pop_block(struct xdag_block *b);

/* initialized block synchronization */
extern int xdag_sync_init(void);

extern int g_xdag_sync_on;

#endif
