/* local storage, T13.663-T13.788 $ DVS: time $ */

#ifndef CHEATCOIN_STORAGE_H
#define CHEATCOIN_STORAGE_H

#include "block.h"

struct cheatcoin_storage_sum {
	uint64_t sum;
	uint64_t size;
};

/* Save the block to local storage, return its number or -1 on error */
extern int64_t cheatcoin_storage_save(const struct cheatcoin_block *b);

/* read from the local repository a block with this number; write it to the buffer or return a permanent reference, 0 on error */
extern struct cheatcoin_block *cheatcoin_storage_load(cheatcoin_hash_t hash, cheatcoin_time_t time, uint64_t pos,
		struct cheatcoin_block *buf);

/* Call a callback for all blocks from the repository that fall with this time interval; returns the number of blocks */
extern uint64_t cheatcoin_load_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *block, void *data));

/* in the sums array puts the sums of blocks along the segment from start to end, divided into 16 parts; end - start must be of the form 16 ^ k */
extern int cheatcoin_load_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* exits the repository */
extern void cheatcoin_storage_finish(void);

#endif
