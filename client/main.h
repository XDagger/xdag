/* basic variables, T13.714-T13.895 $DVS:time$ */

#ifndef CHEATCOIN_MAIN_H
#define CHEATCOIN_MAIN_H

#include <time.h>
#include "block.h"

enum cheatcoin_states
{
#define cheatcoin_state(n,s) CHEATCOIN_STATE_##n ,
#include "state.h"
#undef cheatcoin_state
};

/* the maximum period of time for which blocks are requested, not their amounts */
#define REQUEST_BLOCKS_MAX_TIME	(1 << 20)

extern struct cheatcoin_stats
{
    cheatcoin_diff_t difficulty, max_difficulty;
    uint64_t nblocks, total_nblocks;
    uint64_t nmain, total_nmain;
    uint32_t nhosts, total_nhosts, reserved1, reserved2;
} g_cheatcoin_stats;

#define HASHRATE_LAST_MAX_TIME	(64 * 4)

extern struct cheatcoin_ext_stats
{
    cheatcoin_diff_t hashrate_total[HASHRATE_LAST_MAX_TIME];
    cheatcoin_diff_t hashrate_ours[HASHRATE_LAST_MAX_TIME];
    cheatcoin_time_t hashrate_last_time;
    uint64_t nnoref;
    uint64_t nhashes;
    double hashrate_s;
    uint32_t nwaitsync;
} g_cheatcoin_extstats;

#ifdef __cplusplus
extern "C" {
#endif

    extern void cheatcoin_log_xfer(cheatcoin_hash_t from, cheatcoin_hash_t to, cheatcoin_amount_t amount);

    /* the program state */
    extern int g_cheatcoin_state;

    /* is there command 'run' */
    extern int g_cheatcoin_run;

    /* 1 - the program works in a test network */
    extern int g_cheatcoin_testnet;

    /* coin token and program name */
    extern char *g_coinname, *g_progname;

    /* time of last transfer */
    extern time_t g_cheatcoin_xfer_last;

    extern int cheatcoin_main(int argc, char **argv);

    extern int cheatcoin_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

    extern int cheatcoin_show_state(cheatcoin_hash_t hash);

    extern int(*g_cheatcoin_show_state)(const char *state, const char *balance, const char *address);

    extern int cheatcoin_do_xfer(void *out, const char *amount, const char *address);

#ifdef __cplusplus
};
#endif

#define xdag_amount2xdag(amount) ((unsigned)((amount) >> 32))
#define xdag_amount2cheato(amount) ((unsigned)(((uint64_t)(unsigned)(amount) * 1000000000) >> 32))

#endif
