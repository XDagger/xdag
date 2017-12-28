/* основные переменные, T13.714-T13.775 $DVS:time$ */

#ifndef CHEATCOIN_MAIN_H
#define CHEATCOIN_MAIN_H

#include "block.h"

/* максимальный период времени, за который запрашиваются блоки, а не их суммы */
#define REQUEST_BLOCKS_MAX_TIME	(1 << 20)

extern struct cheatcoin_stats {
	cheatcoin_diff_t difficulty, max_difficulty;
	uint64_t nblocks, total_nblocks;
	uint64_t nmain, total_nmain;
	uint32_t nhosts, total_nhosts, reserved1, reserved2;
} g_cheatcoin_stats;

#define HASHRATE_LAST_MAX_TIME	64

extern struct cheatcoin_ext_stats {
	cheatcoin_diff_t hashrate_total[HASHRATE_LAST_MAX_TIME];
	cheatcoin_diff_t hashrate_ours[HASHRATE_LAST_MAX_TIME];
	cheatcoin_time_t hashrate_last_time;
	uint64_t nnoref;
	uint32_t nwaitsync;
} g_cheatcoin_extstats;

extern void cheatcoin_log_xfer(cheatcoin_hash_t from, cheatcoin_hash_t to, cheatcoin_amount_t amount);

/* 1 - программа работает в тестовой сети */
extern int g_cheatcoin_testnet;

/* имя монеты */
extern char *g_coinname;

#endif
