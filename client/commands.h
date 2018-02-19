#ifndef XDAG_COMMANDS_H
#define XDAG_COMMANDS_H

#include <time.h>
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* time of last transfer */
extern time_t g_cheatcoin_xfer_last;

extern void cheatcoin_log_xfer(cheatcoin_hash_t from, cheatcoin_hash_t to, cheatcoin_amount_t amount);
extern int cheatcoin_do_xfer(void *out, const char *amount, const char *address);
int cheatcoin_show_state(cheatcoin_hash_t hash);

#ifdef __cplusplus
};
#endif

void startCommandProcessing(int transportFlags);
int out_balances();


#endif // !XDAG_COMMANDS_H

