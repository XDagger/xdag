#include "global.h"

int g_xdag_state = XDAG_STATE_INIT;
int g_xdag_testnet = 0;
int g_xdag_run = 0;
enum xdag_field_type g_block_header_type = XDAG_FIELD_HEAD;
struct xdag_stats g_xdag_stats;
struct xdag_ext_stats g_xdag_extstats;
int g_disable_mining = 0;
int g_make_snapshot = 0;
int g_load_snapshot = 0;
uint64_t g_snapshot_time = 0; // time of last snapshot storage file
int g_snapshot_height = 0;
int g_snapshot_compress = 1; // switch of using compress
int g_snapshot_integer = 1; //switch of using integer OR hash as key, using integer has more compression rate of db
int g_snapshot_pub_key = 0; // switch of snapshot pub key
int g_snapshot_balance = 1; // switch of snapshot balance
enum xdag_type g_xdag_type = XDAG_POOL;
enum xdag_mine_type g_xdag_mine_type = XDAG_RAW;
enum randomx_mode g_xdag_rx_mode = RANDOMX_LIGHT;
char *g_coinname, *g_progname;
xdag_time_t g_apollo_fork_time = 0;

#if defined (__MACOS__) || defined (__APPLE__)
inline int is_pool(void) { return g_xdag_type == XDAG_POOL; }
inline int is_wallet(void) { return g_xdag_type == XDAG_WALLET; }
#endif
