/* синхронизация, T13.738-T13.764 $DVS:time$ */

#ifndef CHEATCOIN_SYNC_H
#define CHEATCOIN_SYNC_H

#include "block.h"

/* проверить блок и включить его в базу данных с учётом синхронизации, возвращает не 0 в случае ошибки */
extern int cheatcoin_sync_add_block(struct cheatcoin_block *b, void *conn);

/* извещает механизм синхронизации, что искомый блок уже найден */
extern int cheatcoin_sync_pop_block(struct cheatcoin_block *b);

/* инициализация синхронизации блоков */
extern int cheatcoin_sync_init(void);

extern int g_cheatcoin_sync_on;

#endif
