/* локальное хранилище, T13.663-T13.788 $DVS:time$ */

#ifndef XDAG_STORAGE_H
#define XDAG_STORAGE_H

#include "block.h"

struct xdag_storage_sum {
	uint64_t sum;
	uint64_t size;
};

/* Saves the block to local storage, returns its number or -1 in case of error */
extern int64_t xdag_storage_save(const struct xdag_block *b);

/* reads a block and its number from the local repository; writes it to the buffer or returns a permanent reference, 0 in case of error */
extern struct xdag_block *xdag_storage_load(xdag_hash_t hash, xdag_time_t time, uint64_t pos,
	struct xdag_block *buf);

/* Calls a callback for all blocks from the repository that are in specified time interval; returns the number of blocks */
extern uint64_t xdag_load_blocks(xdag_time_t start_time, xdag_time_t end_time, void *data,
									  void *(*callback)(void *block, void *data));

/* places the sums of blocks in 'sums' array, blocks are filtered by interval from start_time to end_time, splitted to 16 parts;
 * end - start should be in form 16^k
 * (original russian comment is unclear too) */
extern int xdag_load_sums(xdag_time_t start_time, xdag_time_t end_time, struct xdag_storage_sum sums[16]);

/* completes work with the storage */
extern void xdag_storage_finish(void);

#endif
