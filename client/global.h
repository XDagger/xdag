#ifndef XDAG_GLOBAL_H
#define XDAG_GLOBAL_H

#include <stdint.h>
#include "types.h"
#include "time.h"
#include "system.h"
#include "block.h"
#include "rsdb.h"

enum xdag_states {
#define xdag_state(n,s) XDAG_STATE_##n ,
#include "state.h"
#undef xdag_state
};

extern struct xdag_stats {
	xdag_diff_t difficulty, max_difficulty;
	uint64_t nblocks, total_nblocks;
	uint64_t nmain, total_nmain;
	uint32_t nhosts, total_nhosts;
	union {
		uint32_t reserved[2];
		xdag_frame_t main_time;
	};
} g_xdag_stats;

extern struct xdag_ext_stats {
	xdag_diff_t hashrate_total[HASHRATE_LAST_MAX_TIME];
	xdag_diff_t hashrate_ours[HASHRATE_LAST_MAX_TIME];
	xtime_t hashrate_last_time;
	uint64_t nnoref;
	uint64_t nextra;
	uint64_t nhashes;
	double hashrate_s;
	uint32_t nwaitsync;
} g_xdag_extstats;

enum xdag_type {
	XDAG_WALLET = 1,
	XDAG_POOL = 2
};

// defines if xdag started as a pool or a wallet
extern enum xdag_type g_xdag_type;

/* the program state */
extern int g_xdag_state;

/* 1 - the program works in a test network */
extern int g_xdag_testnet;

/* is there command 'run' */
extern int g_xdag_run;

/* coin token and program name */
extern char *g_coinname, *g_progname;

//defines if mining is disabled (pool)
extern int g_disable_mining;

extern xd_rsdb_t* g_xdag_rsdb;

extern xdag_hash_t g_top_main_chain_hash;
extern xdag_hash_t g_pre_top_main_chain_hash;
extern xdag_hashlow_t g_ourfirst_hash;
extern xdag_hashlow_t g_ourlast_hash;
extern xdag_amount_t g_balance;
//Default type of the block header
//Test network and main network have different types of the block headers, so blocks from different networks are incompatible
extern enum xdag_field_type g_block_header_type;

extern int is_pool(void);
extern int is_wallet(void);


#endif
