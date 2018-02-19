#include "commands.h"
#include "main.h"
#include "address.h"
#include "wallet.h"
#include "log.h"
#include "pool.h"
#include "transport.h"
#include <math.h>

#define CHEATCOIN_COMMAND_MAX	0x1000
#define UNIX_SOCK				"unix_sock.dat"
#define XFER_MAX_IN				11
#define Nfields(d) (2 + d->nfields + 3 * d->nkeys + 2 * d->outsig)

struct account_callback_data {
	FILE *out;
	int count;
};

struct xfer_callback_data {
	struct cheatcoin_field fields[XFER_MAX_IN + 1];
	int keys[XFER_MAX_IN + 1];
	cheatcoin_amount_t todo, done, remains;
	int nfields, nkeys, outsig;
};

struct out_balances_data {
	struct cheatcoin_field *blocks;
	unsigned nblocks, maxnblocks;
};

void printHelp(FILE *out);
int account_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key);
int xfer_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key);
long double amount2cheatcoins(cheatcoin_amount_t amount);
long double hashrate(cheatcoin_diff_t *diff);
const char *get_state();

void startCommandProcessing(int transportFlags)
{
	char cmd[CHEATCOIN_COMMAND_MAX];

	if (!(transportFlags & CHEATCOIN_DAEMON)) printf("Type command, help for example.\n");
	for (;;) {
		if (transportFlags & CHEATCOIN_DAEMON) sleep(100);
		else {
			printf("%s> ", g_progname);
			fflush(stdout);
			fgets(cmd, CHEATCOIN_COMMAND_MAX, stdin);
			if (cheatcoin_command(cmd, stdout) < 0) {
				break;
			}
		}
	}
}

int cheatcoin_command(char *cmd, FILE *out)
{
	uint32_t pwd[4];
	char *lasts;
	int ispwd = 0;
	cmd = strtok_r(cmd, " \t\r\n", &lasts);
	if (!cmd) return 0;
	if (sscanf(cmd, "pwd=%8x%8x%8x%8x", pwd, pwd + 1, pwd + 2, pwd + 3) == 4) {
		ispwd = 1;
		cmd = strtok_r(0, " \t\r\n", &lasts);
	}
	if (!strcmp(cmd, "account")) {
		struct account_callback_data d;
		d.out = out;
		d.count = (g_is_miner ? 1 : 20);
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (cmd) {
			sscanf(cmd, "%d", &d.count);
		}
		if (g_cheatcoin_state < CHEATCOIN_STATE_XFER) {
			fprintf(out, "Not ready to show balances. Type 'state' command to see the reason.\n");
		}
		cheatcoin_traverse_our_blocks(&d, &account_callback);
	} else if (!strcmp(cmd, "balance")) {
		if (g_cheatcoin_state < CHEATCOIN_STATE_XFER) {
			fprintf(out, "Not ready to show a balance. Type 'state' command to see the reason.\n");
		} else {
			cheatcoin_hash_t hash;
			cheatcoin_amount_t balance;
			cmd = strtok_r(0, " \t\r\n", &lasts);
			if (cmd) {
				cheatcoin_address2hash(cmd, hash);
				balance = cheatcoin_get_balance(hash);
			} else {
				balance = cheatcoin_get_balance(0);
			}
			fprintf(out, "Balance: %.9Lf %s\n", amount2cheatcoins(balance), g_coinname);
		}
	} else if (!strcmp(cmd, "block")) {
		cheatcoin_hash_t hash;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (cmd) {
			int res = 0, len = strlen(cmd), i, c;
			if (len == 32) {
				if (cheatcoin_address2hash(cmd, hash)) {
					fprintf(out, "Address is incorrect.\n");
					res = -1;
				}
			} else if (len == 48 || len == 64) {
				for (i = 0; i < len; ++i) if (!isxdigit(cmd[i])) {
					fprintf(out, "Hash is incorrect.\n");
					res = -1;
					break;
				}
				if (!res) for (i = 0; i < 24; ++i) {
					sscanf(cmd + len - 2 - 2 * i, "%2x", &c);
					((uint8_t *)hash)[i] = c;
				}
			} else {
				fprintf(out, "Argument is incorrect.\n");
				res = -1;
			}
			if (!res) {
				if (cheatcoin_print_block_info(hash, out)) {
					fprintf(out, "Block is not found.\n");
				}
			}
		} else fprintf(out, "Block is not specified.\n");
	} else if (!strcmp(cmd, "help")) {
		printHelp(out);
	} else if (!strcmp(cmd, "keygen")) {
		int res = cheatcoin_wallet_new_key();
		if (res < 0) {
			fprintf(out, "Can't generate new key pair.\n");
		} else {
			fprintf(out, "Key %d generated and set as default.\n", res);
		}
	} else if (!strcmp(cmd, "level")) {
		unsigned level;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) {
			fprintf(out, "%d\n", cheatcoin_set_log_level(-1));
		} else if (sscanf(cmd, "%u", &level) != 1 || level > CHEATCOIN_TRACE) {
			fprintf(out, "Illegal level.\n");
		} else {
			cheatcoin_set_log_level(level);
		}
	} else if (!strcmp(cmd, "miners")) {
		cheatcoin_print_miners(out);
	} else if (!strcmp(cmd, "mining")) {
		int nthreads;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) {
			fprintf(out, "%d mining threads running\n", g_cheatcoin_mining_threads);
		} else if (sscanf(cmd, "%d", &nthreads) != 1 || nthreads < 0) {
			fprintf(out, "Illegal number.\n");
		} else {
			cheatcoin_mining_start(g_is_miner ? ~nthreads : nthreads);
			fprintf(out, "%d mining threads running\n", g_cheatcoin_mining_threads);
		}
	} else if (!strcmp(cmd, "net")) {
		char netcmd[4096];
		*netcmd = 0;
		while ((cmd = strtok_r(0, " \t\r\n", &lasts))) {
			strcat(netcmd, cmd);
			strcat(netcmd, " ");
		}
		cheatcoin_net_command(netcmd, out);
	} else if (!strcmp(cmd, "pool")) {
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) {
			char buf[0x100];
			cmd = cheatcoin_pool_get_config(buf);
			if (!cmd) {
				fprintf(out, "Pool is disabled.\n");
			} else {
				fprintf(out, "Pool config: %s.\n", cmd);
			}
		} else {
			cheatcoin_pool_set_config(cmd);
		}
	} else if (!strcmp(cmd, "run")) {
		g_cheatcoin_run = 1;
	} else if (!strcmp(cmd, "state")) {
		fprintf(out, "%s\n", get_state());
	} else if (!strcmp(cmd, "stats")) {
		if (g_is_miner) fprintf(out, "your hashrate MHs: %.2lf\n", g_cheatcoin_extstats.hashrate_s / (1024 * 1024));
		else fprintf(out, "Statistics for ours and maximum known parameters:\n"
			"            hosts: %u of %u\n"
			"           blocks: %llu of %llu\n"
			"      main blocks: %llu of %llu\n"
			"    orphan blocks: %llu\n"
			" wait sync blocks: %u\n"
			" chain difficulty: %llx%016llx of %llx%016llx\n"
			" %9s supply: %.9Lf of %.9Lf\n"
			"4 hr hashrate MHs: %.2Lf of %.2Lf\n",
			g_cheatcoin_stats.nhosts, g_cheatcoin_stats.total_nhosts,
			(long long)g_cheatcoin_stats.nblocks, (long long)g_cheatcoin_stats.total_nblocks,
			(long long)g_cheatcoin_stats.nmain, (long long)g_cheatcoin_stats.total_nmain,
			(long long)g_cheatcoin_extstats.nnoref, g_cheatcoin_extstats.nwaitsync,
			cheatcoin_diff_args(g_cheatcoin_stats.difficulty),
			cheatcoin_diff_args(g_cheatcoin_stats.max_difficulty), g_coinname,
			amount2cheatcoins(cheatcoin_get_supply(g_cheatcoin_stats.nmain)),
			amount2cheatcoins(cheatcoin_get_supply(g_cheatcoin_stats.total_nmain)),
			hashrate(g_cheatcoin_extstats.hashrate_ours), hashrate(g_cheatcoin_extstats.hashrate_total)
		);
	} else if (!strcmp(cmd, "exit") || !strcmp(cmd, "terminate")) {
		cheatcoin_wallet_finish();
		cheatcoin_netdb_finish();
		cheatcoin_storage_finish();
		xdag_mem_finish();
		return -1;
	} else if (!strcmp(cmd, "xfer")) {
		char *amount, *address;
		amount = strtok_r(0, " \t\r\n", &lasts);
		if (!amount) {
			fprintf(out, "Xfer: amount not given.\n"); 
			return 1;
		}
		address = strtok_r(0, " \t\r\n", &lasts);
		if (!address) {
			fprintf(out, "Xfer: destination address not given.\n"); 
			return 1;
		}
		if (out == stdout ? cheatcoin_user_crypt_action(0, 0, 0, 3) : (ispwd ? cheatcoin_user_crypt_action(pwd, 0, 4, 5) : 1)) {
			sleep(3); 
			fprintf(out, "Password incorrect.\n");
		} else {
			cheatcoin_do_xfer(out, amount, address);
		}
	} else {
		fprintf(out, "Illegal command.\n");
	}
	return 0;
}

long double diff2log(cheatcoin_diff_t diff)
{
	long double res = (long double)cheatcoin_diff_to64(diff);
	cheatcoin_diff_shr32(&diff);
	cheatcoin_diff_shr32(&diff);
	if (cheatcoin_diff_to64(diff)) {
		res += ldexpl((long double)cheatcoin_diff_to64(diff), 64);
	}
	return (res > 0 ? logl(res) : 0);
}

long double hashrate(cheatcoin_diff_t *diff)
{
	long double sum = 0;
	int i;
	for (i = 0; i < HASHRATE_LAST_MAX_TIME; ++i) {
		sum += diff2log(diff[i]);
	}
	sum /= HASHRATE_LAST_MAX_TIME;
	return ldexpl(expl(sum), -58);
}

const char *get_state()
{
	static const char *states[] = {
#define cheatcoin_state(n,s) s ,
#include "state.h"
#undef cheatcoin_state
	};
	return states[g_cheatcoin_state];
}

cheatcoin_amount_t cheatcoins2amount(const char *str)
{
	long double sum, flr;
	cheatcoin_amount_t res;
	if (sscanf(str, "%Lf", &sum) != 1 || sum <= 0) {
		return 0;
	}
	flr = floorl(sum);
	res = (cheatcoin_amount_t)flr << 32;
	sum -= flr;
	sum = ldexpl(sum, 32);
	flr = ceill(sum);
	return res + (cheatcoin_amount_t)flr;
}

long double amount2cheatcoins(cheatcoin_amount_t amount)
{
	return xdag_amount2xdag(amount) + (long double)xdag_amount2cheato(amount) / 1000000000;
}

int account_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key)
{
	struct account_callback_data *d = (struct account_callback_data *)data;
	if (!d->count--) return -1;
	if (g_cheatcoin_state < CHEATCOIN_STATE_XFER)
		fprintf(d->out, "%s  key %d\n", cheatcoin_hash2address(hash), n_our_key);
	else
		fprintf(d->out, "%s %20.9Lf  key %d\n", cheatcoin_hash2address(hash), amount2cheatcoins(amount), n_our_key);
	return 0;
}

int make_block(struct xfer_callback_data *d)
{
	int res;
	if (d->nfields != XFER_MAX_IN) {
		memcpy(d->fields + d->nfields, d->fields + XFER_MAX_IN, sizeof(cheatcoin_hashlow_t));
	}
	d->fields[d->nfields].amount = d->todo;
	res = cheatcoin_create_block(d->fields, d->nfields, 1, 0, 0);
	if (res) {
		cheatcoin_err("FAILED: to %s xfer %.9Lf %s, error %d",
			cheatcoin_hash2address(d->fields[d->nfields].hash), amount2cheatcoins(d->todo), g_coinname, res);
		return -1;
	}
	d->done += d->todo;
	d->todo = 0;
	d->nfields = 0;
	d->nkeys = 0;
	d->outsig = 1;
	return 0;
}

int cheatcoin_do_xfer(void *outv, const char *amount, const char *address)
{
	struct xfer_callback_data xfer;
	FILE *out = (FILE *)outv;
#ifdef CHEATCOINWALLET
	if (cheatcoin_user_crypt_action(0, 0, 0, 3)) {
		sleep(3); return 1;
	}
#endif
	memset(&xfer, 0, sizeof(xfer));
	xfer.remains = cheatcoins2amount(amount);
	if (!xfer.remains) {
		if (out) fprintf(out, "Xfer: nothing to transfer.\n"); return 1;
	}
	if (xfer.remains > cheatcoin_get_balance(0)) {
		if (out) fprintf(out, "Xfer: balance too small.\n"); return 1;
	}
	if (cheatcoin_address2hash(address, xfer.fields[XFER_MAX_IN].hash)) {
		if (out) fprintf(out, "Xfer: incorrect address.\n"); return 1;
	}
	cheatcoin_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
	xfer.outsig = 1;
	g_cheatcoin_state = CHEATCOIN_STATE_XFER;
	g_cheatcoin_xfer_last = time(0);
	cheatcoin_traverse_our_blocks(&xfer, &xfer_callback);
	if (out) {
		fprintf(out, "Xfer: transferred %.9Lf %s to the address %s, see log for details.\n",
			amount2cheatcoins(xfer.done), g_coinname, cheatcoin_hash2address(xfer.fields[XFER_MAX_IN].hash));
	}
	return 0;
}

int xfer_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key)
{
	struct xfer_callback_data *d = (struct xfer_callback_data *)data;
	cheatcoin_amount_t todo = d->remains;
	int i;
	if (!amount) {
		return -1;
	}
	if (!g_is_miner && cheatcoin_main_time() < (time >> 16) + 2 * CHEATCOIN_POOL_N_CONFIRMATIONS) {
		return 0;
	}
	for (i = 0; i < d->nkeys; ++i) {
		if (n_our_key == d->keys[i]) {
			break;
		}
	}
	if (i == d->nkeys) d->keys[d->nkeys++] = n_our_key;
	if (d->keys[XFER_MAX_IN] == n_our_key) d->outsig = 0;
	if (Nfields(d) > CHEATCOIN_BLOCK_FIELDS) {
		if (make_block(d)) return -1;
		d->keys[d->nkeys++] = n_our_key;
		if (d->keys[XFER_MAX_IN] == n_our_key) {
			d->outsig = 0;
		}
	}
	if (amount < todo) {
		todo = amount;
	}
	memcpy(d->fields + d->nfields, hash, sizeof(cheatcoin_hashlow_t));
	d->fields[d->nfields++].amount = todo;
	d->todo += todo, d->remains -= todo;
	cheatcoin_log_xfer(hash, d->fields[XFER_MAX_IN].hash, todo);
	if (!d->remains || Nfields(d) == CHEATCOIN_BLOCK_FIELDS) {
		if (make_block(d)) {
			return -1;
		}
		if (!d->remains) {
			return 1;
		}
	}
	return 0;
}

void cheatcoin_log_xfer(cheatcoin_hash_t from, cheatcoin_hash_t to, cheatcoin_amount_t amount)
{
	cheatcoin_mess("Xfer  : from %s to %s xfer %.9Lf %s",
		cheatcoin_hash2address(from), cheatcoin_hash2address(to), amount2cheatcoins(amount), g_coinname);
}

int out_balances_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time)
{
	struct out_balances_data *d = (struct out_balances_data *)data;
	struct cheatcoin_field f;
	memcpy(f.hash, hash, sizeof(cheatcoin_hashlow_t));
	f.amount = amount;
	if (!f.amount) {
		return 0;
	}
	if (d->nblocks == d->maxnblocks) {
		d->maxnblocks = (d->maxnblocks ? d->maxnblocks * 2 : 0x100000);
		d->blocks = realloc(d->blocks, d->maxnblocks * sizeof(struct cheatcoin_field));
	}
	memcpy(d->blocks + d->nblocks, &f, sizeof(struct cheatcoin_field));
	d->nblocks++;
	return 0;
}

int out_sort_callback(const void *l, const void *r)
{
	return strcmp(cheatcoin_hash2address(((struct cheatcoin_field *)l)->data),
		cheatcoin_hash2address(((struct cheatcoin_field *)r)->data));
}

void *add_block_callback(void *block, void *data)
{
	unsigned *i = (unsigned *)data;
	cheatcoin_add_block((struct cheatcoin_block *)block);
	if (!(++*i % 10000)) printf("blocks: %u\n", *i);
	return 0;
}

int out_balances()
{
	struct out_balances_data d;
	unsigned i = 0;
	cheatcoin_set_log_level(0);
	xdag_mem_init((cheatcoin_main_time() - cheatcoin_start_main_time()) << 17);
	cheatcoin_crypt_init(0);
	memset(&d, 0, sizeof(struct out_balances_data));
	cheatcoin_load_blocks(cheatcoin_start_main_time() << 16, cheatcoin_main_time() << 16, &i, add_block_callback);
	cheatcoin_traverse_all_blocks(&d, out_balances_callback);
	qsort(d.blocks, d.nblocks, sizeof(struct cheatcoin_field), out_sort_callback);
	for (i = 0; i < d.nblocks; ++i) {
		printf("%s  %20.9Lf\n", cheatcoin_hash2address(d.blocks[i].data), amount2cheatcoins(d.blocks[i].amount));
	}
	return 0;
}

int cheatcoin_show_state(cheatcoin_hash_t hash)
{
	char balance[64], address[64], state[256];
	if (!g_cheatcoin_show_state) {
		return -1;
	}
	if (g_cheatcoin_state < CHEATCOIN_STATE_XFER) {
		strcpy(balance, "Not ready");
	} else {
		sprintf(balance, "%.9Lf", amount2cheatcoins(cheatcoin_get_balance(0)));
	}
	if (!hash) {
		strcpy(address, "Not ready");
	} else {
		strcpy(address, cheatcoin_hash2address(hash));
	}
	strcpy(state, get_state());
	return (*g_cheatcoin_show_state)(state, balance, address);
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
