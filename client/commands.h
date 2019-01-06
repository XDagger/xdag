#ifndef XDAG_COMMANDS_H
#define XDAG_COMMANDS_H

#include <time.h>
#include "block.h"

#define XDAG_COMMAND_MAX	0x1000
#define XDAG_NFIELDS(d) (2 + d->hasRemark + d->fieldsCount + 3 * d->keysCount + 2 * d->outsig)
#define XDAG_COMMAND_HISTORY ".cmd.history"

#ifdef __cplusplus
extern "C" {
#endif

/* time of last transfer */
extern time_t g_xdag_xfer_last;
extern int xdag_do_xfer(void *outv, const char *amount, const char *address, const char *remark, int isGui);
extern void xdagSetCountMiningTread(int miningThreadsCount);
extern double xdagGetHashRate(void);
extern long double hashrate(xdag_diff_t *diff);
extern const char *get_state(void);

#ifdef __cplusplus
};
#endif

#define XFER_MAX_IN				11

struct xfer_callback_data {
	struct xdag_field fields[XFER_MAX_IN + 1];
	int keys[XFER_MAX_IN + 1];
	xdag_amount_t todo, done, remains;
	int fieldsCount, keysCount, outsig, hasRemark;
	xdag_hash_t transactionBlockHash;
	xdag_remark_t remark;
};

struct account_callback_data {
	FILE *out;
	int count;
};

struct out_balances_data {
	struct xdag_field *blocks;
	unsigned blocksCount, maxBlocksCount;
};

typedef int (*xdag_com_func_t)(char*, FILE *);
typedef struct {
	char *name;			    /* command name */
	int avaibility;			/* 0 - both miner and pool, 1 - only miner, 2 - only pool */
	xdag_com_func_t func;   /* command function */
} XDAG_COMMAND;

XDAG_COMMAND* xdag_find_command(char* name);
int xdag_init_commands(void);
int xdag_start_command(int flags);
int xdag_search_command(char *cmd, FILE *out);
int xdag_read_command(char *cmd);

int xdag_com_account(char *, FILE*);
int xdag_com_balance(char *, FILE*);
int xdag_com_block(char *, FILE*);
int xdag_com_lastblocks(char *, FILE*);
int xdag_com_mainblocks(char *, FILE*);
int xdag_com_minedblocks(char *, FILE*);
int xdag_com_orphanblocks(char *, FILE*);
int xdag_com_keygen(char *, FILE*);
int xdag_com_level(char *, FILE*);
int xdag_com_miner(char *, FILE*);
int xdag_com_miners(char *, FILE*);
int xdag_com_mining(char *, FILE*);
int xdag_com_net(char *, FILE*);
int xdag_com_transport(char *, FILE*);
int xdag_com_pool(char *, FILE*);
int xdag_com_stats(char *, FILE*);
int xdag_com_state(char *, FILE*);
int xdag_com_internal_stats(char *, FILE*);
int xdag_com_help(char *, FILE*);
int xdag_com_run(char *, FILE*);
int xdag_com_terminate(char *, FILE*);
int xdag_com_exit(char *, FILE*);
int xdag_com_disconnect(char *, FILE*);
int xdag_com_rpc(char *, FILE*);
int xdag_com_autorefresh(char *, FILE*);
int xdag_com_reload(char *, FILE*);
int xdag_com_xfer(char *nextParam, FILE *out, int ispwd, uint32_t* pwd);

int xdag_log_xfer(xdag_hash_t from, xdag_hash_t to, xdag_amount_t amount);
int xdag_out_balances(void);
int xdag_show_state(xdag_hash_t hash);

int xfer_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time, int n_our_key);

// Function declarations
int account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xtime_t time, int n_our_key);
#endif // !XDAG_COMMANDS_H
