/* локальное хранилище, T13.663-T13.788 $DVS:time$ */

#ifndef CHEATCOIN_STORAGE_H
#define CHEATCOIN_STORAGE_H

#include "block.h"

struct cheatcoin_storage_sum {
	uint64_t sum;
	uint64_t size;
};

/* сохранить блок в локальное хранилище, возвращает его номер или -1 при ошибке */
extern int64_t cheatcoin_storage_save(const struct cheatcoin_block *b);

/* прочитать из локального хранилища блок с данным номером; записать его в буфер или возвратить постоянную ссылку, 0 при ошибке */
extern struct cheatcoin_block *cheatcoin_storage_load(cheatcoin_hash_t hash, cheatcoin_time_t time, uint64_t pos,
		struct cheatcoin_block *buf);

/* вызвать callback для всех блоков из хранилища, попадающих с данный временной интервал; возвращает число блоков */
extern uint64_t cheatcoin_load_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *block, void *data));

/* в массив sums помещает суммы блоков по отрезку от start до end, делённому на 16 частей; end - start должно быть вида 16^k */
extern int cheatcoin_load_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* завершает работу с хранилищем */
extern void cheatcoin_storage_finish(void);

#endif
