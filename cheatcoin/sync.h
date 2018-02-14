/* sync, T13.738-T13.764 $ DVS: time $ */

#ifndef CHEATCOIN_SYNC_H
#define CHEATCOIN_SYNC_H

#include "block.h"

/* check the block and include it in the database with synchronization in mind, it returns not 0 in case of an error */
extern int cheatcoin_sync_add_block(struct cheatcoin_block *b, void *conn);

/* notifies the synchronization mechanism that the desired block is already found */
extern int cheatcoin_sync_pop_block(struct cheatcoin_block *b);

/* initializing block synchronization */
extern int cheatcoin_sync_init(void);

extern int g_cheatcoin_sync_on;

#endif
