/* синхронизация, T13.738-T13.764 $DVS:time$ */

#ifndef CHEATCOIN_SYNC_H
#define CHEATCOIN_SYNC_H

#include "block.h"

/* checks a block and includes it in the database with synchronization, ruturs non-zero value in case of error */
extern int cheatcoin_sync_add_block(struct cheatcoin_block *b, void *conn);

/* notifies synchronization mechanism about found block */
extern int cheatcoin_sync_pop_block(struct cheatcoin_block *b);

/* initialized block synchronization */
extern int cheatcoin_sync_init(void);

extern int g_cheatcoin_sync_on;

#endif
