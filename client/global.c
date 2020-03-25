#include "global.h"

int g_xdag_state = XDAG_STATE_INIT;
int g_xdag_testnet = 0;
int g_xdag_run = 0;
enum xdag_field_type g_block_header_type = XDAG_FIELD_HEAD;
struct xdag_stats g_xdag_stats;
struct xdag_ext_stats g_xdag_extstats;
int g_disable_mining = 0;
enum xdag_type g_xdag_type = XDAG_POOL;
char *g_coinname, *g_progname;
xdag_time_t g_apollo_fork_time = 0;
xd_rsdb_t  *g_xdag_rsdb = NULL;
xdag_hash_t g_top_main_chain_hash = {0};
xdag_hash_t g_pre_top_main_chain_hash = {0};
xdag_hashlow_t g_ourfirst_hash = {0};
xdag_hashlow_t g_ourlast_hash = {0};
xdag_amount_t g_balance = 0;

inline int is_pool(void) { return g_xdag_type == XDAG_POOL; }
inline int is_wallet(void) { return g_xdag_type == XDAG_WALLET; }
