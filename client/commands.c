#include "commands.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "init.h"
#include "address.h"
#include "wallet.h"
#include "log.h"
#include "pool.h"
#include "transport.h"
#include "netdb.h"
#include "memory.h"
#include "crypt.h"
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#define XFER_MAX_IN				11
#define Nfields(d) (2 + d->nfields + 3 * d->nkeys + 2 * d->outsig)

struct account_callback_data {
	FILE *out;
	int count;
};

struct xfer_callback_data {
	struct xdag_field fields[XFER_MAX_IN + 1];
	int keys[XFER_MAX_IN + 1];
	xdag_amount_t todo, done, remains;
	int nfields, nkeys, outsig;
};

struct out_balances_data {
	struct xdag_field *blocks;
	unsigned nblocks, maxnblocks;
};

// Function declarations
void printHelp(FILE *out);
int account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key);
int xfer_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key);
long double amount2xdags(xdag_amount_t amount);
long double hashrate(xdag_diff_t *diff);
const char *get_state(void);
void processAccountCommand(char *nextParam, FILE *out);
void processBalanceCommand(char *nextParam, FILE *out);
void processBlockCommand(char *nextParam, FILE *out);
void processKeyGenCommand(FILE *out);
void processLevelCommand(char *nextParam, FILE *out);
void processMiningCommand(char *nextParam, FILE *out);
void processNetCommand(char *nextParam, FILE *out);
void processPoolCommand(char *nextParam, FILE *out);
void processStatsCommand(FILE *out);
void processExitCommand(void);
void processXferCommand(char *nextParam, FILE *out, int ispwd, uint32_t* pwd);
void processLastBlocksCommand(char *nextParam, FILE *out);
// Function declarations

void startCommandProcessing(int transportFlags)
{
	char cmd[XDAG_COMMAND_MAX];

	if (!(transportFlags & XDAG_DAEMON)) printf("Type command, help for example.\n");
	for (;;) {
		if (transportFlags & XDAG_DAEMON) sleep(100);
		else {
			printf("%s> ", g_progname);
			fflush(stdout);
			fgets(cmd, XDAG_COMMAND_MAX, stdin);
			if (xdag_command(cmd, stdout) < 0) {
				break;
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
	if (!cmd) return 0;
	if (sscanf(cmd, "pwd=%8x%8x%8x%8x", pwd, pwd + 1, pwd + 2, pwd + 3) == 4) {
		ispwd = 1;
		cmd = strtok_r(0, " \t\r\n", &nextParam);
	}
	if (!strcmp(cmd, "account")) {
		processAccountCommand(nextParam, out);
	} else if (!strcmp(cmd, "balance")) {
		processBalanceCommand(nextParam, out);
	} else if (!strcmp(cmd, "block")) {
		processBlockCommand(nextParam, out);
	} else if (!strcmp(cmd, "help")) {
		printHelp(out);
	} else if (!strcmp(cmd, "keygen")) {
		processKeyGenCommand(out);
	} else if (!strcmp(cmd, "level")) {
		processLevelCommand(nextParam, out);
	} else if (!strcmp(cmd, "miners")) {
		xdag_print_miners(out);
	} else if (!strcmp(cmd, "mining")) {
		processMiningCommand(nextParam, out);
	} else if (!strcmp(cmd, "net")) {
		processNetCommand(nextParam, out);
	} else if (!strcmp(cmd, "pool")) {
		processPoolCommand(nextParam, out);
	} else if (!strcmp(cmd, "run")) {
		g_xdag_run = 1;
	} else if (!strcmp(cmd, "state")) {
		fprintf(out, "%s\n", get_state());
	} else if (!strcmp(cmd, "stats")) {
		processStatsCommand(out);
	} else if (!strcmp(cmd, "exit") || !strcmp(cmd, "terminate")) {
		processExitCommand();
		return -1;
	} else if (!strcmp(cmd, "xfer")) {
		processXferCommand(nextParam, out, ispwd, pwd);
	} else if (!strcmp(cmd, "lastblocks")) {
		processLastBlocksCommand(nextParam, out);
	} else {
		fprintf(out, "Illegal command.\n");
	}
	return 0;
}

void processAccountCommand(char *nextParam, FILE *out)
{
	struct account_callback_data d;
	d.out = out;
	d.count = (g_is_miner ? 1 : 20);
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if (cmd) {
		sscanf(cmd, "%d", &d.count);
	}
	if (g_xdag_state < XDAG_STATE_XFER) {
		fprintf(out, "Not ready to show balances. Type 'state' command to see the reason.\n");
	}
	xdag_traverse_our_blocks(&d, &account_callback);
}

void processBalanceCommand(char *nextParam, FILE *out)
{
	if (g_xdag_state < XDAG_STATE_XFER) {
		fprintf(out, "Not ready to show a balance. Type 'state' command to see the reason.\n");
	} else {
		xdag_hash_t hash;
		xdag_amount_t balance;
		char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
		if (cmd) {
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
	if (cmd) {
		int incorrect = 0;
		size_t len = strlen(cmd);

		if (len == 32) {
			if (xdag_address2hash(cmd, hash)) {
				fprintf(out, "Address is incorrect.\n");
				incorrect = -1;
			}
		} else if (len == 48 || len == 64) {
			for (int i = 0; i < len; ++i) {
				if (!isxdigit(cmd[i])) {
					fprintf(out, "Hash is incorrect.\n");
					incorrect = -1;
					break;
				}
			}
			if (!incorrect) {
				for (int i = 0; i < 24; ++i) {
					sscanf(cmd + len - 2 - 2 * i, "%2x", &c);
					((uint8_t *)hash)[i] = c;
				}
			}
		} else {
			fprintf(out, "Argument is incorrect.\n");
			incorrect = -1;
		}
		if (!incorrect) {
			if (xdag_print_block_info(hash, out)) {
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
	if (res < 0) {
		fprintf(out, "Can't generate new key pair.\n");
	} else {
		fprintf(out, "Key %d generated and set as default.\n", res);
	}
}

void processLevelCommand(char *nextParam, FILE *out)
{
	unsigned level;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if (!cmd) {
		fprintf(out, "%d\n", xdag_set_log_level(-1));
	} else if (sscanf(cmd, "%u", &level) != 1 || level > XDAG_TRACE) {
		fprintf(out, "Illegal level.\n");
	} else {
		xdag_set_log_level(level);
	}
}

void processMiningCommand(char *nextParam, FILE *out)
{
	int nthreads;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if (!cmd) {
		fprintf(out, "%d mining threads running\n", g_xdag_mining_threads);
	} else if (sscanf(cmd, "%d", &nthreads) != 1 || nthreads < 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		xdag_mining_start(g_is_miner ? ~nthreads : nthreads);
		fprintf(out, "%d mining threads running\n", g_xdag_mining_threads);
	}
}

void processNetCommand(char *nextParam, FILE *out)
{
	char *cmd;
	char netcmd[4096];
	*netcmd = 0;
	while ((cmd = strtok_r(nextParam, " \t\r\n", &nextParam))) {
		strcat(netcmd, cmd);
		strcat(netcmd, " ");
	}
	xdag_net_command(netcmd, out);
}

void processPoolCommand(char *nextParam, FILE *out)
{
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if (!cmd) {
		char buf[0x100];
		cmd = xdag_pool_get_config(buf);
		if (!cmd) {
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
	if (g_is_miner) {
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
	if (!amount) {
		fprintf(out, "Xfer: amount not given.\n");
		return;
	}
	char *address = strtok_r(0, " \t\r\n", &nextParam);
	if (!address) {
		fprintf(out, "Xfer: destination address not given.\n");
		return;
	}
	if (out == stdout ? xdag_user_crypt_action(0, 0, 0, 3) : (ispwd ? xdag_user_crypt_action(pwd, 0, 4, 5) : 1)) {
		sleep(3);
		fprintf(out, "Password incorrect.\n");
	} else {
		xdag_do_xfer(out, amount, address, 0);
	}
}

void processLastBlocksCommand(char *nextParam, FILE *out)
{
	int blocksCount;
	char *cmd = strtok_r(nextParam, " \t\r\n", &nextParam);
	if (!cmd) {
		fprintf(out, "Blocks count is not specified.\n");
	} else if (sscanf(cmd, "%d", &blocksCount) != 1 || blocksCount <= 0) {
		fprintf(out, "Illegal number.\n");
	} else {
		//100 is limit
		if (blocksCount > 100) {
			blocksCount = 100;
		}
		char** addressList = xdagCreateStringArray(blocksCount, 40);	//lets assume max address length as 39 symbols + null terminator
		const int retrievedBlocks = xdagGetLastMainBlocks(blocksCount, addressList);
		for (int i = 0; i < retrievedBlocks; ++i) {
			fprintf(out, "%s\n", addressList[i]);
		}
		xdagFreeStringArray(addressList, blocksCount);
	}
}

static long double diff2log(xdag_diff_t diff)
{
	long double res = (long double)xdag_diff_to64(diff);
	xdag_diff_shr32(&diff);
	xdag_diff_shr32(&diff);
	if (xdag_diff_to64(diff)) {
		res += ldexpl((long double)xdag_diff_to64(diff), 64);
	}
	return (res > 0 ? logl(res) : 0);
}

long double hashrate(xdag_diff_t *diff)
{
	long double sum = 0;
	for (int i = 0; i < HASHRATE_LAST_MAX_TIME; ++i) {
		sum += diff2log(diff[i]);
	}
	sum /= HASHRATE_LAST_MAX_TIME;
	return ldexpl(expl(sum), -58);
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

static xdag_amount_t xdags2amount(const char *str)
{
	long double sum;
	if (sscanf(str, "%Lf", &sum) != 1 || sum <= 0) {
		return 0;
	}
	long double flr = floorl(sum);
	xdag_amount_t res = (xdag_amount_t)flr << 32;
	sum -= flr;
	sum = ldexpl(sum, 32);
	flr = ceill(sum);
	return res + (xdag_amount_t)flr;
}

long double amount2xdags(xdag_amount_t amount)
{
	return xdag_amount2xdag(amount) + (long double)xdag_amount2cheato(amount) / 1000000000;
}

int account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	struct account_callback_data *d = (struct account_callback_data *)data;
	if (!d->count--) return -1;
	if (g_xdag_state < XDAG_STATE_XFER)
		fprintf(d->out, "%s  key %d\n", xdag_hash2address(hash), n_our_key);
	else
		fprintf(d->out, "%s %20.9Lf  key %d\n", xdag_hash2address(hash), amount2xdags(amount), n_our_key);
	return 0;
}

static int make_block(struct xfer_callback_data *d)
{
	if (d->nfields != XFER_MAX_IN) {
		memcpy(d->fields + d->nfields, d->fields + XFER_MAX_IN, sizeof(xdag_hashlow_t));
	}
	d->fields[d->nfields].amount = d->todo;
	int res = xdag_create_block(d->fields, d->nfields, 1, 0, 0);
	if (res) {
		xdag_err("FAILED: to %s xfer %.9Lf %s, error %d",
			xdag_hash2address(d->fields[d->nfields].hash), amount2xdags(d->todo), g_coinname, res);
		return -1;
	}
	d->done += d->todo;
	d->todo = 0;
	d->nfields = 0;
	d->nkeys = 0;
	d->outsig = 1;
	return 0;
}

int xdag_do_xfer(void *outv, const char *amount, const char *address, int isGui)
{
	struct xfer_callback_data xfer;
	FILE *out = (FILE *)outv;
	
	if (isGui && xdag_user_crypt_action(0, 0, 0, 3)) {
		sleep(3); 
		return 1;
	}

	memset(&xfer, 0, sizeof(xfer));
	xfer.remains = xdags2amount(amount);
	if (!xfer.remains) {
		if (out) {
			fprintf(out, "Xfer: nothing to transfer.\n");
		}
		return 1;
	}
	if (xfer.remains > xdag_get_balance(0)) {
		if (out) {
			fprintf(out, "Xfer: balance too small.\n");
		}
		return 1;
	}
	if (xdag_address2hash(address, xfer.fields[XFER_MAX_IN].hash)) {
		if (out) {
			fprintf(out, "Xfer: incorrect address.\n");
		}
		return 1;
	}
	xdag_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
	xfer.outsig = 1;
	g_xdag_state = XDAG_STATE_XFER;
	g_xdag_xfer_last = time(0);
	xdag_traverse_our_blocks(&xfer, &xfer_callback);
	if (out) {
		fprintf(out, "Xfer: transferred %.9Lf %s to the address %s, see log for details.\n",
			amount2xdags(xfer.done), g_coinname, xdag_hash2address(xfer.fields[XFER_MAX_IN].hash));
	}
	return 0;
}

int xfer_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	struct xfer_callback_data *d = (struct xfer_callback_data *)data;
	xdag_amount_t todo = d->remains;
	int i;
	if (!amount) {
		return -1;
	}
	if (!g_is_miner && xdag_main_time() < (time >> 16) + 2 * XDAG_POOL_N_CONFIRMATIONS) {
		return 0;
	}
	for (i = 0; i < d->nkeys; ++i) {
		if (n_our_key == d->keys[i]) {
			break;
		}
	}
	if (i == d->nkeys) d->keys[d->nkeys++] = n_our_key;
	if (d->keys[XFER_MAX_IN] == n_our_key) d->outsig = 0;
	if (Nfields(d) > XDAG_BLOCK_FIELDS) {
		if (make_block(d)) return -1;
		d->keys[d->nkeys++] = n_our_key;
		if (d->keys[XFER_MAX_IN] == n_our_key) {
			d->outsig = 0;
		}
	}
	if (amount < todo) {
		todo = amount;
	}
	memcpy(d->fields + d->nfields, hash, sizeof(xdag_hashlow_t));
	d->fields[d->nfields++].amount = todo;
	d->todo += todo, d->remains -= todo;
	xdag_log_xfer(hash, d->fields[XFER_MAX_IN].hash, todo);
	if (!d->remains || Nfields(d) == XDAG_BLOCK_FIELDS) {
		if (make_block(d)) {
			return -1;
		}
		if (!d->remains) {
			return 1;
		}
	}
	return 0;
}

void xdag_log_xfer(xdag_hash_t from, xdag_hash_t to, xdag_amount_t amount)
{
	xdag_mess("Xfer  : from %s to %s xfer %.9Lf %s",
		xdag_hash2address(from), xdag_hash2address(to), amount2xdags(amount), g_coinname);
}

static int out_balances_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time)
{
	struct out_balances_data *d = (struct out_balances_data *)data;
	struct xdag_field f;
	memcpy(f.hash, hash, sizeof(xdag_hashlow_t));
	f.amount = amount;
	if (!f.amount) {
		return 0;
	}
	if (d->nblocks == d->maxnblocks) {
		d->maxnblocks = (d->maxnblocks ? d->maxnblocks * 2 : 0x100000);
		d->blocks = realloc(d->blocks, d->maxnblocks * sizeof(struct xdag_field));
	}
	memcpy(d->blocks + d->nblocks, &f, sizeof(struct xdag_field));
	d->nblocks++;
	return 0;
}

static int out_sort_callback(const void *l, const void *r)
{
	return strcmp(xdag_hash2address(((struct xdag_field *)l)->data),
		xdag_hash2address(((struct xdag_field *)r)->data));
}

static void *add_block_callback(void *block, void *data)
{
	unsigned *i = (unsigned *)data;
	xdag_add_block((struct xdag_block *)block);
	if (!(++*i % 10000)) printf("blocks: %u\n", *i);
	return 0;
}

int out_balances()
{
	struct out_balances_data d;
	unsigned i = 0;
	xdag_set_log_level(0);
	xdag_mem_init((xdag_main_time() - xdag_start_main_time()) << 17);
	xdag_crypt_init(0);
	memset(&d, 0, sizeof(struct out_balances_data));
	xdag_load_blocks(xdag_start_main_time() << 16, xdag_main_time() << 16, &i, add_block_callback);
	xdag_traverse_all_blocks(&d, out_balances_callback);
	qsort(d.blocks, d.nblocks, sizeof(struct xdag_field), out_sort_callback);
	for (i = 0; i < d.nblocks; ++i) {
		printf("%s  %20.9Lf\n", xdag_hash2address(d.blocks[i].data), amount2xdags(d.blocks[i].amount));
	}
	return 0;
}

int xdag_show_state(xdag_hash_t hash)
{
	char balance[64], address[64], state[256];
	if (!g_xdag_show_state) {
		return -1;
	}
	if (g_xdag_state < XDAG_STATE_XFER) {
		strcpy(balance, "Not ready");
	} else {
		sprintf(balance, "%.9Lf", amount2xdags(xdag_get_balance(0)));
	}
	if (!hash) {
		strcpy(address, "Not ready");
	} else {
		strcpy(address, xdag_hash2address(hash));
	}
	strcpy(state, get_state());
	return (*g_xdag_show_state)(state, balance, address);
}

void printHelp(FILE *out)
{
	fprintf(out, "Commands:\n"
		"  account [N] - print first N (20 by default) our addresses with their amounts\n"
		"  balance [A] - print balance of the address A or total balance for all our addresses\n"
		"  block [A]   - print extended info for the block corresponding to the address or hash A\n"
		"  exit        - exit this program (not the daemon)\n"
		"  help        - print this help\n"
		"  keygen      - generate new private/public key pair and set it by default\n"
		"  level [N]   - print level of logging or set it to N (0 - nothing, ..., 9 - all)\n"
		"  miners      - for pool, print list of recent connected miners\n"
		"  mining [N]  - print number of mining threads or set it to N\n"
		"  net command - run transport layer command, try 'net help'\n"
		"  pool [CFG]  - print or set pool config; CFG is miners:fee:reward:direct:maxip:fund\n"
		"  run         - run node after loading local blocks if option -r is used\n"
		"  state       - print the program state\n"
		"  stats       - print statistics for loaded and all known blocks\n"
		"  terminate   - terminate both daemon and this program\n"
		"  xfer S A    - transfer S our %s to the address A\n"
		, g_coinname);
}

void xdagSetCountMiningTread(int miningThreadsCount)
{
	xdag_mining_start(~miningThreadsCount);
}

double xdagGetHashRate(void)
{
	return g_xdag_extstats.hashrate_s / (1024 * 1024);
}
