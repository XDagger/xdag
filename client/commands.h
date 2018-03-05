#ifndef XDAG_COMMANDS_H
#define XDAG_COMMANDS_H

#include <time.h>
#include "block.h"

#define XDAG_COMMAND_MAX	0x1000

#ifdef __cplusplus
extern "C" {
#endif

/* time of last transfer */
extern time_t g_xdag_xfer_last;
extern int xdag_do_xfer(void *outv, const char *amount, const char *address, int isGui);
extern void xdagSetCountMiningTread(int miningThreadsCount);
extern double xdagGetHashRate(void);

#ifdef __cplusplus
};
#endif

void startCommandProcessing(int transportFlags);
int xdag_command(char *cmd, FILE *out);
void xdag_log_xfer(xdag_hash_t from, xdag_hash_t to, xdag_amount_t amount);
int out_balances(void);
int xdag_show_state(xdag_hash_t hash);

#endif // !XDAG_COMMANDS_H
