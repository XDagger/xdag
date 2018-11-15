/* basic variables, T13.714-T14.582 $DVS:time$ */

#ifndef XDAG_MAIN_H
#define XDAG_MAIN_H

#include <time.h>
#include "time.h"
#include "block.h"
#include "system.h"

enum xdag_states
{
#define xdag_state(n,s) XDAG_STATE_##n ,
#include "state.h"
#undef xdag_state
};

extern struct xdag_stats
{
	xdag_diff_t difficulty, max_difficulty;
	uint64_t nblocks, total_nblocks;
	uint64_t nmain, total_nmain;
	uint32_t nhosts, total_nhosts;
	union {
		uint32_t reserved[2];
		uint64_t main_time;
	};
} g_xdag_stats;

extern struct xdag_ext_stats
{
	xdag_diff_t hashrate_total[HASHRATE_LAST_MAX_TIME];
	xdag_diff_t hashrate_ours[HASHRATE_LAST_MAX_TIME];
	xtime_t hashrate_last_time;
	uint64_t nnoref;
	uint64_t nextra;
	uint64_t nhashes;
	double hashrate_s;
	uint32_t nwaitsync;
	uint32_t cache_size;
	uint32_t cache_usage;
	double cache_hitrate;
	int use_orphan_hashtable;
} g_xdag_extstats;

#ifdef __cplusplus
extern "C" {
#endif

/* the program state */
extern int g_xdag_state;

/* is there command 'run' */
extern int g_xdag_run;

/* 1 - the program works in a test network */
extern int g_xdag_testnet;

/* coin token and program name */
extern char *g_coinname, *g_progname;

//defines if client runs as miner or pool
extern int g_is_miner;

//defines if mining is disabled (pool)
extern int g_disable_mining;

//Default type of the block header
//Test network and main network have different types of the block headers, so blocks from different networks are incompatible
extern enum xdag_field_type g_block_header_type;

extern int xdag_init(int argc, char **argv, int isGui);

extern int xdag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

extern int(*g_xdag_show_state)(const char *state, const char *balance, const char *address);

#ifdef __cplusplus
};
#endif

#endif
