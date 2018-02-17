/* локальное хранилище, T13.663-T13.788 $DVS:time$ */

#ifndef CHEATCOIN_STORAGE_H
#define CHEATCOIN_STORAGE_H

#include "block.h"

struct cheatcoin_storage_sum {
	uint64_t sum;
	uint64_t size;
};

/* Saves the block to local storage, returns its number or -1 in case of error */
extern int64_t cheatcoin_storage_save(const struct cheatcoin_block *b);

/* reads a block and its number from the local repository; writes it to the buffer or returns a permanent reference, 0 in case of error */
extern struct cheatcoin_block *cheatcoin_storage_load(cheatcoin_hash_t hash, cheatcoin_time_t time, uint64_t pos,
		struct cheatcoin_block *buf);

/* Calls a callback for all blocks from the repository that are in specified time interval; returns the number of blocks */
extern uint64_t cheatcoin_load_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *block, void *data));

/* в массив sums помещает суммы блоков по отрезку от start до end, делённому на 16 частей; end - start должно быть вида 16^k */
extern int cheatcoin_load_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* завершает работу с хранилищем */
extern void cheatcoin_storage_finish(void);

#endif
