#ifndef XDAG_COMMANDS_H
#define XDAG_COMMANDS_H

#include <time.h>
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* time of last transfer */
extern time_t g_xdag_xfer_last;

extern void xdag_log_xfer(xdag_hash_t from, xdag_hash_t to, xdag_amount_t amount);
extern int xdag_do_xfer(void *out, const char *amount, const char *address);
int xdag_show_state(xdag_hash_t hash);

#ifdef __cplusplus
};
#endif

void startCommandProcessing(int transportFlags);
int out_balances();


#endif // !XDAG_COMMANDS_H

