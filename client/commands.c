#include "commands.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "init.h"
#include "address.h"
#include "wallet.h"
#include "utils/log.h"
#include "pool.h"
#include "miner.h"
#include "transport.h"
#include "netdb.h"
#include "memory.h"
#include "crypt.h"
#if !defined(_WIN32) && !defined(_WIN64)
#include "utils/linenoise.h"
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#define Nfields(d) (2 + d->fieldsCount + 3 * d->keysCount + 2 * d->outsig)
#define COMMAND_HISTORY ".cmd.history"

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
	char *name;				/* command name */
	int avaibility;         /* 0 - both miner and pool, 1 - only miner, 2 - only pool */
	xdag_com_func_t func;	/* command function */
} XDAG_COMMAND;

// Function declarations
int account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key);
long double hashrate(xdag_diff_t *diff);
const char *get_state(void);

void processAccountCommand(char *nextParam, FILE *out);
void processBalanceCommand(char *nextParam, FILE *out);
void processBlockCommand(char *nextParam, FILE *out);
void processKeyGenCommand(FILE *out);
void processLevelCommand(char *nextParam, FILE *out);
void processMinerCommand(char *nextParam, FILE *out);
void processMinersCommand(char *nextParam, FILE *out);
void processMiningCommand(char *nextParam, FILE *out);
void processNetCommand(char *nextParam, FILE *out);
void processPoolCommand(char *nextParam, FILE *out);
void processStatsCommand(FILE *out);
void processCacheCommand(FILE *out);
void processExitCommand(void);
void processXferCommand(char *nextParam, FILE *out, int ispwd, uint32_t* pwd);
void processLastBlocksCommand(char *nextParam, FILE *out);
void processMainBlocksCommand(char *nextParam, FILE *out);
void processMinedBlocksCommand(char *nextParam, FILE *out);
void processHelpCommand(FILE *out);
void processDisconnectCommand(char *nextParam, FILE *out);

int xdag_com_account(char *, FILE*);
int xdag_com_balance(char *, FILE*);
int xdag_com_block(char *, FILE*);
int xdag_com_lastblocks(char *, FILE*);
int xdag_com_mainblocks(char *, FILE*);
int xdag_com_minedblocks(char *, FILE*);
int xdag_com_keyGen(char *, FILE*);
int xdag_com_level(char *, FILE*);
int xdag_com_miner(char *, FILE*);
int xdag_com_miners(char *, FILE*);
int xdag_com_mining(char *, FILE*);
int xdag_com_net(char *, FILE*);
int xdag_com_pool(char *, FILE*);
int xdag_com_stats(char *, FILE*);
int xdag_com_state(char *, FILE*);
int xdag_com_cache(char *, FILE*);
int xdag_com_help(char *, FILE*);
int xdag_com_run(char *, FILE*);
int xdag_com_terminate(char *, FILE*);
int xdag_com_exit(char *, FILE*);
int xdag_com_disconnect(char *, FILE*);

XDAG_COMMAND* find_xdag_command(char*);

XDAG_COMMAND commands[] = {
	{ "account"    , 0, xdag_com_account },
	{ "balance"    , 0, xdag_com_balance },
	{ "block"      , 2, xdag_com_block },
	{ "lastblocks" , 2, xdag_com_lastblocks },
	{ "mainblocks" , 2, xdag_com_mainblocks },
	{ "minedblocks", 2, xdag_com_minedblocks },
	{ "keyGen"     , 0, xdag_com_keyGen },
	{ "level"      , 0, xdag_com_level },
	{ "miner"      , 2, xdag_com_miner },
	{ "miners"     , 2, xdag_com_miners },
	{ "mining"     , 1, xdag_com_mining },
	{ "net"        , 0, xdag_com_net },
	{ "pool"       , 2, xdag_com_pool },
	{ "run"        , 0, xdag_com_run },
	{ "state"      , 0, xdag_com_state },
	{ "stats"      , 0, xdag_com_stats },
	{ "cache"      , 2, xdag_com_cache },
	{ "terminate"  , 0, xdag_com_terminate },
	{ "exit"       , 0, xdag_com_exit },
	{ "xfer"       , 0, (xdag_com_func_t)NULL},
	{ "help"       , 0, xdag_com_help},
	{ "disconnect" , 2, xdag_com_disconnect },
	{ (char *)NULL , 0, (xdag_com_func_t)NULL}
};

int xdag_com_account(char* args, FILE* out)
{
	processAccountCommand(args, out);
	return 0;
}

int xdag_com_balance(char * args, FILE* out)
{
	processBalanceCommand(args, out);
	return 0;
}

int xdag_com_block(char * args, FILE* out)
{
	processBlockCommand(args, out);
	return 0;
}

int xdag_com_lastblocks(char * args, FILE* out)
{
	processLastBlocksCommand(args, out);
	return 0;
}

int xdag_com_mainblocks(char * args, FILE* out)
{
	processMainBlocksCommand(args, out);
	return 0;
}

int xdag_com_minedblocks(char * args, FILE* out)
{
	processMinedBlocksCommand(args, out);
	return 0;
}

int xdag_com_keyGen(char * args, FILE* out)
{
	processKeyGenCommand(out);
	return 0;
}

int xdag_com_level(char * args, FILE* out)
{
	processLevelCommand(args, out);
	return 0;
}

int xdag_com_mining(char * args, FILE* out)
{
	processMiningCommand(args, out);
	return 0;
}

int xdag_com_net(char * args, FILE* out)
{
	processNetCommand(args, out);
	return 0;
}

int xdag_com_pool(char * args, FILE* out)
{
	processPoolCommand(args, out);
	return 0;
}

int xdag_com_miner(char * args, FILE* out)
{
	processMinerCommand(args, out);
	return 0;
}

int xdag_com_miners(char * args, FILE* out)
{
	processMinersCommand(args, out);
	return 0;
}

int xdag_com_stats(char * args, FILE* out)
{
	processStatsCommand(out);
	return 0;
}

int xdag_com_state(char * args, FILE* out)
{
	fprintf(out, "%s\n", get_state());
	return 0;
}

int xdag_com_cache(char * args, FILE* out)
{
	processCacheCommand(out);
	return 0;
}


int xdag_com_run(char * args, FILE* out)
{
	g_xdag_run = 1;
	return 0;
}

int xdag_com_terminate(char * args, FILE* out)
{
	processExitCommand();
	return -1;
}

int xdag_com_exit(char * args, FILE* out)
{
	processExitCommand();
	return -1;
}

int xdag_com_help(char *args, FILE* out)
{
	processHelpCommand(out);
	return 0;
}

int xdag_com_disconnect(char *args, FILE *out)
{
	processDisconnectCommand(args, out);
	return 0;
}

XDAG_COMMAND* find_xdag_command(char *name)
{
	for(int i = 0; commands[i].name; i++) {
		if(strcmp(name, commands[i].name) == 0) {
			return (&commands[i]);
		}
	}
	return (XDAG_COMMAND *)NULL;
}

void startCommandProcessing(int transportFlags)
{
	char cmd[XDAG_COMMAND_MAX];
	if(!(transportFlags & XDAG_DAEMON)) printf("Type command, help for example.\n");

	xdag_init_commands();

	for(;;) {
		if(transportFlags & XDAG_DAEMON) {
			sleep(100);
		} else {
			read_command(cmd);
			if(strlen(cmd) > 0) {
				int ret = xdag_command(cmd, stdout);
				if(ret < 0) {
					break;
				}
			}
		}
	}
}

int xdag_command(char *cmd, FILE *out)
{
	uint32_t pwd[4];
	char *nextParam;
	int ispwd = 0;

	cmd = strtok_r(cmd, " \t\r\n", &nextParam);
	if(!cmd) return 0;
	if(sscanf(cmd, "pwd=%8x%8x%8x%8x", pwd, pwd + 1, pwd + 2, pwd + 3) == 4) {
		ispwd = 1;
		cmd = strtok_r(0, " \t\r\n", &nextParam);
	}

	XDAG_COMMAND *command = find_xdag_command(cmd);

	if(!command || (command->avaibility == 1 && !g_is_miner) || (command->avaibility == 2 && g_is_miner)) {
		fprintf(out, "Illegal command.\n");
	} else {
		if(!strcmp(command->name, "xfer")) {
			processXferCommand(nextParam, out, ispwd, pwd);
		} else {
			return (*(command->func))(nextParam, out);
		}
	}
	return 0;
}

void processAccountCommand(char *nextParam, FILE *out)
{
	struct account_callback_data d;
	d.out = out;
	d.count = (g_is_miner ? 1 : 20);
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(cmd) {
		sscanf(cmd, "%d", &d.count);
	}
	if(g_xdag_state < XDAG_STATE_XFER) {
		fprintf(out, "Not ready to show balances. Type 'state' command to see the reason.\n");
	}
	xdag_traverse_our_blocks(&d, &account_callback);
}

void processBalanceCommand(char *nextParam, FILE *out)
{
	if(g_xdag_state < XDAG_STATE_XFER) {
		fprintf(out, "Not ready to show a balance. Type 'state' command to see the reason.\n");
	} else {
		xdag_hash_t hash;
		xdag_amount_t balance;
		char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
		if(cmd) {
			xdag_address2hash(cmd, hash);
			balance = xdag_get_balance(hash);
		} else {
			balance = xdag_get_balance(0);
		}
		fprintf(out, "Balance: %.9Lf %s\n", amount2xdags(balance), g_coinname);
	}
}

void processBlockCommand(char *nextParam, FILE *out)
{
	int c;
	xdag_hash_t hash;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(cmd) {
		int incorrect = 0;
		size_t len = strlen(cmd);

		if(len == 32) {
			if(xdag_address2hash(cmd, hash)) {
				fprintf(out, "Address is incorrect.\n");
				incorrect = -1;
			}
		} else if(len == 48 || len == 64) {
			for(int i = 0; i < len; ++i) {
				if(!isxdigit(cmd[i])) {
					fprintf(out, "Hash is incorrect.\n");
					incorrect = -1;
					break;
				}
			}
			if(!incorrect) {
				for(int i = 0; i < 24; ++i) {
					sscanf(cmd + len - 2 - 2 * i, "%2x", &c);
					((uint8_t *)hash)[i] = c;
				}
			}
		} else {
			fprintf(out, "Argument is incorrect.\n");
			incorrect = -1;
		}
		if(!incorrect) {
			if(xdag_print_block_info(hash, out)) {
				fprintf(out, "Block is not found.\n");
			}
		}
	} else {
		fprintf(out, "Block is not specified.\n");
	}
}

void processKeyGenCommand(FILE *out)
{
	const int res = xdag_wallet_new_key();
	if(res < 0) {
		fprintf(out, "Can't generate new key pair.\n");
	} else {
		fprintf(out, "Key %d generated and set as default.\n", res);
	}
}

void processLevelCommand(char *nextParam, FILE *out)
{
	unsigned level;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(!cmd) {
		fprintf(out, "%d\n", xdag_set_log_level(-1));
	} else if(sscanf(cmd, "%u", &level) != 1 || level > XDAG_TRACE) {
		fprintf(out, "Illegal level.\n");
	} else {
		xdag_set_log_level(level);
	}
}

void processMiningCommand(char *nextParam, FILE *out)
{
	int nthreads;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(!cmd) {
		fprintf(out, "%d mining threads running\n", g_xdag_mining_threads);
	} else if(sscanf(cmd, "%d", &nthreads) != 1 || nthreads < 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		xdag_mining_start(nthreads);
		fprintf(out, "%d mining threads running\n", g_xdag_mining_threads);
	}
}

void processMinerCommand(char *nextParam, FILE *out)
{
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(cmd) {
		size_t len = strlen(cmd);
		if(len == 32) {
			if(!xdag_print_miner_stats(cmd, out)) {
				fprintf(out, "Miner is not found.\n");
			}
		} else {
			fprintf(out, "Argument is incorrect.\n");
		}
	} else {
		fprintf(out, "Miner is not specified.\n");
	}
}

void processMinersCommand(char *nextParam, FILE *out)
{
	int printOnlyConnections = 0;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(cmd) {
		printOnlyConnections = strcmp(cmd, "conn") == 0;
	}
	xdag_print_miners(out, printOnlyConnections);
}

void processNetCommand(char *nextParam, FILE *out)
{
	char *cmd;
	char netcmd[4096];
	*netcmd = 0;
	while((cmd = strtok_r(nextParam, " \t\r\n", &nextParam))) {
		strcat(netcmd, cmd);
		strcat(netcmd, " ");
	}
	xdag_net_command(netcmd, out);
}

void processPoolCommand(char *nextParam, FILE *out)
{
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(!cmd) {
		char buf[0x100];
		cmd = xdag_pool_get_config(buf);
		if(!cmd) {
			fprintf(out, "Pool is disabled.\n");
		} else {
			fprintf(out, "Pool config: %s.\n", cmd);
		}
	} else {
		xdag_pool_set_config(cmd);
	}
}

void processStatsCommand(FILE *out)
{
	if(g_is_miner) {
		fprintf(out, "your hashrate MHs: %.2lf\n", xdagGetHashRate());
	} else {
		fprintf(out, "Statistics for ours and maximum known parameters:\n"
			"            hosts: %u of %u\n"
			"           blocks: %llu of %llu\n"
			"      main blocks: %llu of %llu\n"
			"    orphan blocks: %llu\n"
			" wait sync blocks: %u\n"
			" chain difficulty: %llx%016llx of %llx%016llx\n"
			" %9s supply: %.9Lf of %.9Lf\n"
			"4 hr hashrate MHs: %.2Lf of %.2Lf\n",
			g_xdag_stats.nhosts, g_xdag_stats.total_nhosts,
			(long long)g_xdag_stats.nblocks, (long long)g_xdag_stats.total_nblocks,
			(long long)g_xdag_stats.nmain, (long long)g_xdag_stats.total_nmain,
			(long long)g_xdag_extstats.nnoref, g_xdag_extstats.nwaitsync,
			xdag_diff_args(g_xdag_stats.difficulty),
			xdag_diff_args(g_xdag_stats.max_difficulty), g_coinname,
			amount2xdags(xdag_get_supply(g_xdag_stats.nmain)),
			amount2xdags(xdag_get_supply(g_xdag_stats.total_nmain)),
			hashrate(g_xdag_extstats.hashrate_ours), hashrate(g_xdag_extstats.hashrate_total)
		);
	}
}

void processCacheCommand(FILE *out)
{
	fprintf(out, "Cache informations:\n"
		"     cached blocks: target amount %u, actual amount %u, hitrate %f%%\n",
		g_xdag_extstats.cache_size, g_xdag_extstats.cache_usage, g_xdag_extstats.cache_hitrate*100
	);
}


void processExitCommand()
{
	xdag_wallet_finish();
	xdag_netdb_finish();
	xdag_storage_finish();
	xdag_mem_finish();
}

void processXferCommand(char *nextParam, FILE *out, int ispwd, uint32_t* pwd)
{
	char *amount = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(!amount) {
		fprintf(out, "Xfer: amount not given.\n");
		return;
	}
	char *address = strtok_r(0, " \t\r\n", &nextParam);
	if(!address) {
		fprintf(out, "Xfer: destination address not given.\n");
		return;
	}
	if(out == stdout ? xdag_user_crypt_action(0, 0, 0, 3) : (ispwd ? xdag_user_crypt_action(pwd, 0, 4, 5) : 1)) {
		sleep(3);
		fprintf(out, "Password incorrect.\n");
	} else {
		xdag_do_xfer(out, amount, address, 0);
	}
}

void processLastBlocksCommand(char *nextParam, FILE *out)
{
	int blocksCount = 20;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if((cmd && sscanf(cmd, "%d", &blocksCount) != 1) || blocksCount <= 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		xdag_list_main_blocks(blocksCount, 1, out);
	}
}

void processMainBlocksCommand(char *nextParam, FILE *out)
{
	int blocksCount = 20;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if((cmd && sscanf(cmd, "%d", &blocksCount) != 1) || blocksCount <= 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		xdag_list_main_blocks(blocksCount, 0, out);
	}
}

void processMinedBlocksCommand(char *nextParam, FILE *out)
{
	int blocksCount = 20;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if((cmd && sscanf(cmd, "%d", &blocksCount) != 1) || blocksCount <= 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		xdag_list_mined_blocks(blocksCount, 0, out);
	}
}

void processDisconnectCommand(char *nextParam, FILE *out)
{
	char *typestr = strtok_r(nextParam, " \t\r\n", &nextParam);
	if(!typestr) {
		fprintf(out, "Invalid parameter.\n");
		return;
	}

	enum disconnect_type type = 0;
	char *value = NULL;
	if(strcmp(typestr, "all") == 0) {
		type = DISCONNECT_ALL;
	} else if(strcmp(typestr, "address") == 0) {
		type = DISCONNECT_BY_ADRESS;
	} else if(strcmp(typestr, "ip") == 0) {
		type = DISCONNECT_BY_IP;
	}

	if(type == 0) {
		fprintf(out, "Invalid parameter.\n");
		return;
	}

	if(type == DISCONNECT_BY_ADRESS || type == DISCONNECT_BY_IP) {
		value = strtok_r(nextParam, " \t\r\n", &nextParam);
		if(!value) {
			fprintf(out, "Invalid parameter.\n");
			return;
		}
	}

	disconnect_connections(type, value);
}

long double hashrate(xdag_diff_t *diff)
{
	long double sum = 0;
	for(int i = 0; i < HASHRATE_LAST_MAX_TIME; ++i) {
		sum += diff2log(diff[i]);
	}
	sum /= HASHRATE_LAST_MAX_TIME;
	return ldexpl(expl(sum), -58); //shown pool and network hashrate seems to be around 35% higher than real, to consider *(0.65) about correction. Deeper study is needed.
}

const char *get_state()
{
	static const char *states[] = {
#define xdag_state(n,s) s ,
#include "state.h"
#undef xdag_state
	};
	return states[g_xdag_state];
}

int account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	char address[33];
	struct account_callback_data *d = (struct account_callback_data *)data;
	if(!d->count--) {
		return -1;
	}
	xdag_hash2address(hash, address);
	if(g_xdag_state < XDAG_STATE_XFER)
		fprintf(d->out, "%s  key %d\n", address, n_our_key);
	else
		fprintf(d->out, "%s %20.9Lf  key %d\n", address, amount2xdags(amount), n_our_key);
	return 0;
}

static int make_transaction_block(struct xfer_callback_data *xferData)
{
	char address[33];
	if(xferData->fieldsCount != XFER_MAX_IN) {
		memcpy(xferData->fields + xferData->fieldsCount, xferData->fields + XFER_MAX_IN, sizeof(xdag_hashlow_t));
	}
	xferData->fields[xferData->fieldsCount].amount = xferData->todo;
	int res = xdag_create_block(xferData->fields, xferData->fieldsCount, 1, 0, 0, xferData->transactionBlockHash);
	if(res) {
		xdag_hash2address(xferData->fields[xferData->fieldsCount].hash, address);
		xdag_err("FAILED: to %s xfer %.9Lf %s, error %d",
			address, amount2xdags(xferData->todo), g_coinname, res);
		return -1;
	}
	xferData->done += xferData->todo;
	xferData->todo = 0;
	xferData->fieldsCount = 0;
	xferData->keysCount = 0;
	xferData->outsig = 1;
	return 0;
}

int xdag_do_xfer(void *outv, const char *amount, const char *address, int isGui)
{
	char address_buf[33];
	struct xfer_callback_data xfer;
	FILE *out = (FILE *)outv;

	if(isGui && xdag_user_crypt_action(0, 0, 0, 3)) {
		sleep(3);
		return 1;
	}

	memset(&xfer, 0, sizeof(xfer));
	xfer.remains = xdags2amount(amount);
	if(!xfer.remains) {
		if(out) {
			fprintf(out, "Xfer: nothing to transfer.\n");
		}
		return 1;
	}
	if(xfer.remains > xdag_get_balance(0)) {
		if(out) {
			fprintf(out, "Xfer: balance too small.\n");
		}
		return 1;
	}
	if(xdag_address2hash(address, xfer.fields[XFER_MAX_IN].hash)) {
		if(out) {
			fprintf(out, "Xfer: incorrect address.\n");
		}
		return 1;
	}
	xdag_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
	xfer.outsig = 1;
	g_xdag_state = XDAG_STATE_XFER;
	g_xdag_xfer_last = time(0);
	xdag_traverse_our_blocks(&xfer, &xfer_callback);
	if(out) {
		xdag_hash2address(xfer.fields[XFER_MAX_IN].hash, address_buf);
		fprintf(out, "Xfer: transferred %.9Lf %s to the address %s.\n", amount2xdags(xfer.done), g_coinname, address_buf);
		xdag_hash2address(xfer.transactionBlockHash, address_buf);
		fprintf(out, "Transaction address is %s, it will take several minutes to complete the transaction.\n", address_buf);
	}
	return 0;
}

int xfer_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	struct xfer_callback_data *xferData = (struct xfer_callback_data*)data;
	xdag_amount_t todo = xferData->remains;
	int i;
	if(!amount) {
		return -1;
	}
	if(!g_is_miner && xdag_main_time() < (time >> 16) + 2 * CONFIRMATIONS_COUNT) {
		return 0;
	}
	for(i = 0; i < xferData->keysCount; ++i) {
		if(n_our_key == xferData->keys[i]) {
			break;
		}
	}
	if(i == xferData->keysCount) {
		xferData->keys[xferData->keysCount++] = n_our_key;
	}
	if(xferData->keys[XFER_MAX_IN] == n_our_key) {
		xferData->outsig = 0;
	}
	if(Nfields(xferData) > XDAG_BLOCK_FIELDS) {
		if(make_transaction_block(xferData)) {
			return -1;
		}
		xferData->keys[xferData->keysCount++] = n_our_key;
		if(xferData->keys[XFER_MAX_IN] == n_our_key) {
			xferData->outsig = 0;
		}
	}
	if(amount < todo) {
		todo = amount;
	}
	memcpy(xferData->fields + xferData->fieldsCount, hash, sizeof(xdag_hashlow_t));
	xferData->fields[xferData->fieldsCount++].amount = todo;
	xferData->todo += todo;
	xferData->remains -= todo;
	xdag_log_xfer(hash, xferData->fields[XFER_MAX_IN].hash, todo);
	if(!xferData->remains || Nfields(xferData) == XDAG_BLOCK_FIELDS) {
		if(make_transaction_block(xferData)) {
			return -1;
		}
		if(!xferData->remains) {
			return 1;
		}
	}
	return 0;
}

void xdag_log_xfer(xdag_hash_t from, xdag_hash_t to, xdag_amount_t amount)
{
	char address_from[33], address_to[33];
	xdag_hash2address(from, address_from);
	xdag_hash2address(to, address_to);
	xdag_mess("Xfer : from %s to %s xfer %.9Lf %s", address_from, address_to, amount2xdags(amount), g_coinname);
}

static int out_balances_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time)
{
	struct out_balances_data *d = (struct out_balances_data *)data;
	struct xdag_field f;
	memcpy(f.hash, hash, sizeof(xdag_hashlow_t));
	f.amount = amount;
	if(!f.amount) {
		return 0;
	}
	if(d->blocksCount == d->maxBlocksCount) {
		d->maxBlocksCount = (d->maxBlocksCount ? d->maxBlocksCount * 2 : 0x100000);
		d->blocks = realloc(d->blocks, d->maxBlocksCount * sizeof(struct xdag_field));
	}
	memcpy(d->blocks + d->blocksCount, &f, sizeof(struct xdag_field));
	d->blocksCount++;
	return 0;
}

static int out_sort_callback(const void *l, const void *r)
{
	char address_l[33], address_r[33];
	xdag_hash2address(((struct xdag_field *)l)->data, address_l);
	xdag_hash2address(((struct xdag_field *)r)->data, address_r);
	return strcmp(address_l, address_r);
}

static void *add_block_callback(void *block, void *data)
{
	unsigned *i = (unsigned *)data;
	xdag_add_block((struct xdag_block *)block);
	if(!(++*i % 10000)) printf("blocks: %u\n", *i);
	return 0;
}

int out_balances()
{
	char address[33];
	struct out_balances_data d;
	unsigned i = 0;
	xdag_set_log_level(0);
	xdag_mem_init((xdag_main_time() - xdag_start_main_time()) << 17);
	xdag_crypt_init(0);
	memset(&d, 0, sizeof(struct out_balances_data));
	xdag_load_blocks(xdag_start_main_time() << 16, xdag_main_time() << 16, &i, &add_block_callback);
	xdag_traverse_all_blocks(&d, out_balances_callback);
	qsort(d.blocks, d.blocksCount, sizeof(struct xdag_field), out_sort_callback);
	for(i = 0; i < d.blocksCount; ++i) {
		xdag_hash2address(d.blocks[i].data, address);
		printf("%s  %20.9Lf\n", address, amount2xdags(d.blocks[i].amount));
	}
	return 0;
}

int xdag_show_state(xdag_hash_t hash)
{
	char balance[64], address[64], state[256];
	if(!g_xdag_show_state) {
		return -1;
	}
	if(g_xdag_state < XDAG_STATE_XFER) {
		strcpy(balance, "Not ready");
	} else {
		sprintf(balance, "%.9Lf", amount2xdags(xdag_get_balance(0)));
	}
	if(!hash) {
		strcpy(address, "Not ready");
	} else {
		xdag_hash2address(hash, address);
	}
	strcpy(state, get_state());
	return (*g_xdag_show_state)(state, balance, address);
}

void processHelpCommand(FILE *out)
{
	fprintf(out, "Commands:\n"
		"  account [N]         - print first N (20 by default) our addresses with their amounts\n"
		"  balance [A]         - print balance of the address A or total balance for all our addresses\n"
		"  block [A]           - print extended info for the block corresponding to the address or hash A\n"
		"  lastblocks [N]      - print latest N (20 by default, max limit 100) addresses of main blocks\n"
		"  exit                - exit this program (not the daemon)\n"
		"  help                - print this help\n"
		"  keygen              - generate new private/public key pair and set it by default\n"
		"  level [N]           - print level of logging or set it to N (0 - nothing, ..., 9 - all)\n"
		"  miners              - for pool, print list of recent connected miners\n"
		"  mining [N]          - print number of mining threads or set it to N\n"
		"  net command         - run transport layer command, try 'net help'\n"
		"  pool [CFG]          - print or set pool config; CFG is miners:maxip:maxconn:fee:reward:direct:fund\n"
		"                         miners - maximum allowed number of miners,\n"
		"                         maxip - maximum allowed number of miners connected from single ip,\n"
		"                         maxconn - maximum allowed number of miners with the same address,\n"
		"                         fee - pool fee in percent,\n"
		"                         reward - reward to miner who got a block in percent,\n"
		"                         direct - reward to miners participated in earned block in percent,\n"
		"                         fund - community fund fee in percent\n"
		"  run                 - run node after loading local blocks if option -r is used\n"
		"  state               - print the program state\n"
		"  stats               - print statistics for loaded and all known blocks\n"
		"  terminate           - terminate both daemon and this program\n"
		"  xfer S A            - transfer S our %s to the address A\n"
		"  disconnect O [A|IP] - disconnect all connections or specified miners\n"
		"                         O is option, can be all, address or ip\n"
		"                         A is the miners' address\n"
		"                         IP is the miners' IP\n"
		"  mainblocks [N]       - print list of N (20 by default) main blocks\n"
		"  minedblocks [N]      - print list of N (20 by default) main blocks mined by current pool\n"
		, g_coinname);
}

void xdagSetCountMiningTread(int miningThreadsCount)
{
	xdag_mining_start(miningThreadsCount);
}

double xdagGetHashRate(void)
{
	return g_xdag_extstats.hashrate_s / (1024 * 1024);
}

int read_command(char *cmd)
{
#if !defined(_WIN32) && !defined(_WIN64)
	char* line = linenoise("xdag> ");
	if(line == NULL) return 0;

	if(strlen(line) > XDAG_COMMAND_MAX) {
		printf("exceed max length\n");
		strncpy(cmd, line, XDAG_COMMAND_MAX - 1);
		cmd[XDAG_COMMAND_MAX - 1] = '\0';
	} else {
		strcpy(cmd, line);
	}
	free(line);

	if(strlen(cmd) > 0) {
		linenoiseHistoryAdd(cmd);
		linenoiseHistorySave(COMMAND_HISTORY);
	}
#else
	printf("%s> ", g_progname);
	fflush(stdout);
	fgets(cmd, XDAG_COMMAND_MAX, stdin);
#endif

	return 0;
}

#if !defined(_WIN32) && !defined(_WIN64)
static void xdag_com_completion(const char *buf, linenoiseCompletions *lc)
{
	for(int index = 0; commands[index].name; index++) {
		if(!strncmp(buf, commands[index].name, strlen(buf))) {
			linenoiseAddCompletion(lc, commands[index].name);
		}
	}
}
#endif

void xdag_init_commands(void)
{
#if !defined(_WIN32) && !defined(_WIN64)
	linenoiseSetCompletionCallback(xdag_com_completion); //set completion
	linenoiseHistorySetMaxLen(50); //set max line for history
	linenoiseHistoryLoad(COMMAND_HISTORY); //load history
#endif
}
