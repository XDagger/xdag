#include "global.h"

int g_xdag_state = XDAG_STATE_INIT;
int g_xdag_testnet = 1;
int g_xdag_run = 0;
enum xdag_field_type g_block_header_type = XDAG_FIELD_HEAD_TEST;
struct xdag_stats g_xdag_stats;
struct xdag_ext_stats g_xdag_extstats;
int g_disable_mining = 0;
enum xdag_type g_xdag_type = XDAG_POOL;
char *g_coinname, *g_progname;
XDAG_RSDB* g_xdag_rsdb = NULL;

#if defined (__MACOS__) || defined (__APPLE__)
inline int is_pool(void) { return g_xdag_type == XDAG_POOL; }
inline int is_wallet(void) { return g_xdag_type == XDAG_WALLET; }
#endif


