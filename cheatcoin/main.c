/* cheatcoin main, T13.654-T13.847 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include "address.h"
#include "block.h"
#include "crypt.h"
#include "log.h"
#include "transport.h"
#include "version.h"
#include "wallet.h"
#include "netdb.h"
#include "main.h"
#include "sync.h"
#include "pool.h"
#include "memory.h"

#define CHEATCOIN_COMMAND_MAX	0x1000
#define FIFO_IN					"fifo_cmd.dat"
#define FIFO_OUT				"fifo_res.dat"
#define XFER_MAX_IN				11

char *g_coinname, *g_progname;
#define coinname				g_coinname

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

int g_cheatcoin_state = CHEATCOIN_STATE_INIT;
int g_cheatcoin_testnet = 0, g_is_miner = 0;
static int g_is_pool = 0;
int g_cheatcoin_run = 0;
time_t g_cheatcoin_xfer_last = 0;
struct cheatcoin_stats g_cheatcoin_stats;
struct cheatcoin_ext_stats g_cheatcoin_extstats;
int (*g_cheatcoin_show_state)(const char *state, const char *balance, const char *address) = 0;

static long double amount2cheatcoins(cheatcoin_amount_t amount) {
	long double res = 0, d = 1;
	int i;
	for (i = 0; i < 32 || (amount && i < 64); ++i, amount >>= 1) {
		if (amount & 1) res += d;
		if (i < 32) res /= 2; else d *= 2;
	}
	return floorl(res * 1000000000) / 1000000000;
}

static long double diff2log(cheatcoin_diff_t diff) {
	long double res = (long double)cheatcoin_diff_to64(diff);
	cheatcoin_diff_shr32(&diff);
	cheatcoin_diff_shr32(&diff);
	if (cheatcoin_diff_to64(diff)) res += ldexpl((long double)cheatcoin_diff_to64(diff), 64);
	return (res > 0 ? logl(res) : 0);
}

static long double hashrate(cheatcoin_diff_t *diff) {
	long double sum = 0;
	int i;
	for (i = 0; i < HASHRATE_LAST_MAX_TIME; ++i) sum += diff2log(diff[i]);
	sum /= HASHRATE_LAST_MAX_TIME;
	return ldexpl(expl(sum), -58);
}

static cheatcoin_amount_t cheatcoins2amount(const char *str) {
	long double sum, flr;
	cheatcoin_amount_t res;
	int i;
	if (sscanf(str, "%Lf", &sum) != 1 || sum <= 0) return 0;
	flr = floorl(sum);
	res = (cheatcoin_amount_t)flr << 32;
	sum -= flr;
	for (i = 31; i >= 0; --i) {
		sum *= 2;
		if (sum >= 1) res |= 1ll << i, sum--;
	}
	return res;
}

static int account_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key) {
	struct account_callback_data *d = (struct account_callback_data *)data;
	if (!d->count--) return -1;
	if (g_cheatcoin_state < CHEATCOIN_STATE_XFER)
		fprintf(d->out, "%s  key %d\n", cheatcoin_hash2address(hash), n_our_key);
	else
		fprintf(d->out, "%s %20.9Lf  key %d\n", cheatcoin_hash2address(hash), amount2cheatcoins(amount), n_our_key);
	return 0;
}

static int make_block(struct xfer_callback_data *d) {
	int res;
	if (d->nfields != XFER_MAX_IN) memcpy(d->fields + d->nfields, d->fields + XFER_MAX_IN, sizeof(cheatcoin_hashlow_t));
	d->fields[d->nfields].amount = d->todo;
	res = cheatcoin_create_block(d->fields, d->nfields, 1, 0, 0);
	if (res) {
		cheatcoin_err("FAILED: to %s xfer %.9Lf %s, error %d",
				cheatcoin_hash2address(d->fields[d->nfields].hash), amount2cheatcoins(d->todo), coinname, res);
		return -1;
	}
	d->done += d->todo;
	d->todo = 0;
	d->nfields = 0;
	d->nkeys = 0;
	d->outsig = 1;
	return 0;
}

#define Nfields(d) (2 + d->nfields + 3 * d->nkeys + 2 * d->outsig)

void cheatcoin_log_xfer(cheatcoin_hash_t from, cheatcoin_hash_t to, cheatcoin_amount_t amount) {
	cheatcoin_mess("Xfer  : from %s to %s xfer %.9Lf %s",
			cheatcoin_hash2address(from), cheatcoin_hash2address(to), amount2cheatcoins(amount), coinname);
}

static int xfer_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time, int n_our_key) {
	struct xfer_callback_data *d = (struct xfer_callback_data *)data;
	cheatcoin_amount_t todo = d->remains;
	int i;
	if (!amount) return -1;
	if (g_is_pool && cheatcoin_main_time() < (time >> 16) + 2 * CHEATCOIN_POOL_N_CONFIRMATIONS) return 0;
	for (i = 0; i < d->nkeys; ++i) if (n_our_key == d->keys[i]) break;
	if (i == d->nkeys) d->keys[d->nkeys++] = n_our_key;
	if (d->keys[XFER_MAX_IN] == n_our_key) d->outsig = 0;
	if (Nfields(d) > CHEATCOIN_BLOCK_FIELDS) {
		if (make_block(d)) return -1;
		d->keys[d->nkeys++] = n_our_key;
		if (d->keys[XFER_MAX_IN] == n_our_key) d->outsig = 0;
	}
	if (amount < todo) todo = amount;
	memcpy(d->fields + d->nfields, hash, sizeof(cheatcoin_hashlow_t));
	d->fields[d->nfields++].amount = todo;
	d->todo += todo, d->remains -= todo;
	cheatcoin_log_xfer(hash, d->fields[XFER_MAX_IN].hash, todo);
	if (!d->remains || Nfields(d) == CHEATCOIN_BLOCK_FIELDS) {
		if (make_block(d)) return -1;
		if (!d->remains) return 1;
	}
	return 0;
}

static const char *get_state(void) {
	static const char *states[] = {
#define cheatcoin_state(n,s) s ,
#include "state.h"
#undef cheatcoin_state
	};
	return states[g_cheatcoin_state];
}

int cheatcoin_do_xfer(void *outv, const char *amount, const char *address) {
	struct xfer_callback_data xfer;
	FILE *out = (FILE *)outv;
#ifdef CHEATCOINWALLET
	if (cheatcoin_user_crypt_action(0, 0, 0, 3)) {
		sleep(3); return 1;
	}
#endif
	memset(&xfer, 0, sizeof(xfer));
	xfer.remains = cheatcoins2amount(amount);
	if (!xfer.remains) { if (out) fprintf(out, "Xfer: nothing to transfer.\n"); return 1; }
	if (xfer.remains > cheatcoin_get_balance(0)) { if (out) fprintf(out, "Xfer: balance too small.\n"); return 1; }
	if (cheatcoin_address2hash(address, xfer.fields[XFER_MAX_IN].hash))
		{ if (out) fprintf(out, "Xfer: incorrect address.\n"); return 1; }
	cheatcoin_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
	xfer.outsig = 1;
	g_cheatcoin_state = CHEATCOIN_STATE_XFER;
	g_cheatcoin_xfer_last = time(0);
	cheatcoin_traverse_our_blocks(&xfer, &xfer_callback);
	if (out) fprintf(out, "Xfer: transferred %.9Lf %s to the address %s, see log for details.\n",
		amount2cheatcoins(xfer.done), coinname, cheatcoin_hash2address(xfer.fields[XFER_MAX_IN].hash));
	return 0;
}

static int cheatcoin_command(char *cmd, FILE *out) {
	uint32_t pwd[4];
	char *lasts;
	int ispwd = 0;
	cmd = strtok_r(cmd, " \t\r\n", &lasts);
	if (!cmd) return 0;
	if (sscanf(cmd, "pwd=%8x%8x%8x%8x", pwd, pwd+1, pwd+2, pwd+3) == 4) { ispwd = 1, cmd = strtok_r(0, " \t\r\n", &lasts); }
	if (!strcmp(cmd, "account")) {
		struct account_callback_data d;
		d.out = out;
		d.count = (g_is_miner ? 1 : 20);
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (cmd) sscanf(cmd, "%d", &d.count);
		if (g_cheatcoin_state < CHEATCOIN_STATE_XFER)
			fprintf(out, "Not ready to show balances. Type 'state' command to see the reason.\n");
		cheatcoin_traverse_our_blocks(&d, &account_callback);
	} else if (!strcmp(cmd, "balance")) {
		if (g_cheatcoin_state < CHEATCOIN_STATE_XFER)
			fprintf(out, "Not ready to show a balance. Type 'state' command to see the reason.\n");
		else {
			cheatcoin_hash_t hash;
			cheatcoin_amount_t balance;
			cmd = strtok_r(0, " \t\r\n", &lasts);
			if (cmd) {
				cheatcoin_address2hash(cmd, hash);
				balance = cheatcoin_get_balance(hash);
			} else balance = cheatcoin_get_balance(0);
			fprintf(out, "Balance: %.9Lf %s\n", amount2cheatcoins(balance), coinname);
		}
	} else if (!strcmp(cmd, "help")) {
		fprintf(out, "Commands:\n"
			"  account [N] - print first N (20 by default) our addresses with their amounts\n"
			"  balance [A] - print balance of the address A or total balance for all our addresses\n"
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
		, coinname);
	} else if (!strcmp(cmd, "keygen")) {
		int res = cheatcoin_wallet_new_key();
		if (res < 0) fprintf(out, "Can't generate new key pair.\n");
		else fprintf(out, "Key %d generated and set as default.\n", res);
	} else if (!strcmp(cmd, "level")) {
		unsigned level;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) fprintf(out, "%d\n", cheatcoin_set_log_level(-1));
		else if (sscanf(cmd, "%u", &level) != 1 || level > CHEATCOIN_TRACE) fprintf(out, "Illegal level.\n");
		else cheatcoin_set_log_level(level);
	} else if (!strcmp(cmd, "miners")) {
		cheatcoin_print_miners(out);
	} else if (!strcmp(cmd, "mining")) {
		int nthreads;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) fprintf(out, "%d mining threads running\n", g_cheatcoin_mining_threads);
		else if (sscanf(cmd, "%d", &nthreads) != 1 || nthreads < 0) fprintf(out, "Illegal number.\n");
		else {
			cheatcoin_mining_start(g_is_miner ? ~nthreads : nthreads);
			fprintf(out, "%d mining threads running\n", g_cheatcoin_mining_threads);
		}
	} else if (!strcmp(cmd, "net")) {
		char netcmd[4096];
		*netcmd = 0;
		while ((cmd = strtok_r(0, " \t\r\n", &lasts))) { strcat(netcmd, cmd); strcat(netcmd, " "); }
		cheatcoin_net_command(netcmd, out);
	} else if (!strcmp(cmd, "pool")) {
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) {
			char buf[0x100];
			cmd = cheatcoin_pool_get_config(buf);
			if (!cmd) fprintf(out, "Pool is disabled.\n");
			else fprintf(out, "Pool config: %s.\n", cmd);
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
			"hour hashrate MHs: %.2Lf of %.2Lf\n",
			g_cheatcoin_stats.nhosts, g_cheatcoin_stats.total_nhosts,
			(long long)g_cheatcoin_stats.nblocks, (long long)g_cheatcoin_stats.total_nblocks,
			(long long)g_cheatcoin_stats.nmain, (long long)g_cheatcoin_stats.total_nmain,
			(long long)g_cheatcoin_extstats.nnoref, g_cheatcoin_extstats.nwaitsync,
			cheatcoin_diff_args(g_cheatcoin_stats.difficulty),
			cheatcoin_diff_args(g_cheatcoin_stats.max_difficulty), coinname,
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
		if (!amount) { fprintf(out, "Xfer: amount not given.\n"); return 1; }
		address = strtok_r(0, " \t\r\n", &lasts);
		if (!address) { fprintf(out, "Xfer: destination address not given.\n"); return 1; }
		if (out == stdout ? cheatcoin_user_crypt_action(0, 0, 0, 3) : (ispwd ? cheatcoin_user_crypt_action(pwd, 0, 4, 5) : 1)) {
			sleep(3); fprintf(out, "Password incorrect.\n");
		} else cheatcoin_do_xfer(out, amount, address);
	} else {
		fprintf(out, "Illegal command.\n");
	}
	return 0;
}

struct out_balances_data {
	struct cheatcoin_field *blocks;
	unsigned nblocks, maxnblocks;
};

static int out_balances_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, cheatcoin_time_t time) {
	struct out_balances_data *d = (struct out_balances_data *)data;
	struct cheatcoin_field f;
	memcpy(f.hash, hash, sizeof(cheatcoin_hashlow_t));
	f.amount = amount;
	if (!f.amount) return 0;
	if (d->nblocks == d->maxnblocks) {
		d->maxnblocks = (d->maxnblocks ? d->maxnblocks * 2 : 0x100000);
		d->blocks = realloc(d->blocks, d->maxnblocks * sizeof(struct cheatcoin_field));
	}
	memcpy(d->blocks + d->nblocks, &f, sizeof(struct cheatcoin_field));
	d->nblocks++;
	return 0;
}

static int out_sort_callback(const void *l, const void *r) {
	return strcmp(cheatcoin_hash2address(((struct cheatcoin_field *)l)->data),
				  cheatcoin_hash2address(((struct cheatcoin_field *)r)->data));
}

static void *add_block_callback(void *block, void *data) {
	unsigned *i = (unsigned *)data;
	cheatcoin_add_block((struct cheatcoin_block *)block);
	if (!(++*i % 10000)) printf("blocks: %u\n", *i);
	return 0;
}

static int out_balances(void) {
	struct out_balances_data d;
	unsigned i = 0;
	cheatcoin_set_log_level(0);
	xdag_mem_init((cheatcoin_main_time() - cheatcoin_start_main_time()) << 17);
	cheatcoin_crypt_init(0);
	memset(&d, 0, sizeof(struct out_balances_data));
	cheatcoin_load_blocks(cheatcoin_start_main_time() << 16, cheatcoin_main_time() << 16, &i, add_block_callback);
	cheatcoin_traverse_all_blocks(&d, out_balances_callback);
	qsort(d.blocks, d.nblocks, sizeof(struct cheatcoin_field), out_sort_callback);
	for (i = 0; i < d.nblocks; ++i)
		printf("%s  %20.9Lf\n", cheatcoin_hash2address(d.blocks[i].data), amount2cheatcoins(d.blocks[i].amount));
	return 0;
}

static int terminal(void) {
#if !defined(_WIN32) && !defined(_WIN64)
	char cmd[CHEATCOIN_COMMAND_MAX], cmd2[CHEATCOIN_COMMAND_MAX], *ptr, *lasts;
	int fd;
	while(1) {
		int ispwd = 0, c = 0;
		printf("%s> ", g_progname); fflush(stdout);
		fgets(cmd, CHEATCOIN_COMMAND_MAX, stdin);
		strcpy(cmd2, cmd);
		ptr = strtok_r(cmd2, " \t\r\n", &lasts);
		if (!ptr) continue;
		if (!strcmp(ptr, "exit")) break;
		if (!strcmp(ptr, "xfer")) {
			uint32_t pwd[4];
			cheatcoin_user_crypt_action(pwd, 0, 4, 4);
			sprintf(cmd2, "pwd=%08x%08x%08x%08x ", pwd[0], pwd[1], pwd[2], pwd[3]);
			ispwd = 1;
		}
		fd = open(FIFO_IN, O_WRONLY);
		if (fd < 0) { printf("Can't open pipe.\n"); continue; }
		if (ispwd) write(fd, cmd2, strlen(cmd2));
		write(fd, cmd, strlen(cmd) + 1);
		close(fd);
		fd = open(FIFO_OUT, O_RDONLY);
		if (fd < 0) { printf("Can't open pipe.\n"); continue; }
		while (read(fd, &c, 1) == 1 && c) putchar(c);
		close(fd);
		if (!strcmp(ptr, "terminate")) break;
	}
#endif
	return 0;
}

static void *terminal_thread(void *arg) {
#if !defined(_WIN32) && !defined(_WIN64)
	char cmd[CHEATCOIN_COMMAND_MAX];
	int pos, in, out, c, res;
	FILE *fout;
	mkfifo(FIFO_IN, 0660);
	mkfifo(FIFO_OUT, 0660);
	cheatcoin_info("Terminal thread entered main cycle");
	while (1) {
		in = open(FIFO_IN, O_RDONLY); if (in < 0) { cheatcoin_err("Can't open " FIFO_IN); break; }
		out = open(FIFO_OUT, O_WRONLY); if (out < 0) { cheatcoin_err("Can't open " FIFO_OUT); break; }
		fout = fdopen(out, "w"); if (!fout) { cheatcoin_err("Can't fdopen " FIFO_OUT); break; }
		for (pos = 0; pos < CHEATCOIN_COMMAND_MAX - 1 && read(in, &c, 1) == 1 && c; ++pos) cmd[pos] = c;
		cmd[pos] = 0;
		res = cheatcoin_command(cmd, fout);
		fputc(0, fout);
		fflush(fout);
		fclose(fout);
//		close(out);
		close(in);
		if (res < 0) exit(0);
		sleep(1);
	}
#endif
	return 0;
}

#ifdef CHEATCOINWALLET
int cheatcoin_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
	const char *addrports[256], *bindto = 0, *pubaddr = 0, *pool_arg = 0, *miner_address = 0;
	char *ptr;
	int transport_flags = 0, n_addrports = 0, n_mining_threads = 0, is_pool = 0, is_miner = 0, i, level;
	pthread_t th;
#if !defined(_WIN32) && !defined(_WIN64)
	signal(SIGPIPE, SIG_IGN);
	signal(SIGWINCH, SIG_IGN);
#endif
	g_progname = strdup(argv[0]);
	while ((ptr = strchr(g_progname, '/')) || (ptr = strchr(g_progname, '\\'))) g_progname = ptr + 1;
	if ((ptr = strchr(g_progname, '.'))) *ptr = 0;
	for (ptr = g_progname; *ptr; ptr++) *ptr = tolower((unsigned char)*ptr);
	coinname = strdup(g_progname);
	for (ptr = coinname; *ptr; ptr++) *ptr = toupper((unsigned char)*ptr);
#ifndef CHEATCOINWALLET
	printf("%s client/server, version %s.\n", g_progname, CHEATCOIN_VERSION);
#endif
	g_cheatcoin_run = 1;
	cheatcoin_show_state(0);
	if (argc <= 1) goto help;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] && !argv[i][2]) switch(argv[i][1]) {
			case 'a':
				if (++i < argc) miner_address = argv[i];
				break;
			case 'c':
				if (++i < argc && n_addrports < 256)
					addrports[n_addrports++] = argv[i];
				break;
			case 'd':
#if !defined(_WIN32) && !defined(_WIN64)
				transport_flags |= CHEATCOIN_DAEMON;
#endif
				break;
			case 'h':
			help:
				printf("Usage: %s flags [pool_ip:port]\n"
					"If pool_ip:port argument is given, then the node operates as a miner.\n"
					"Flags:\n"
					"  -a address     - specify your address to use in the miner\n"
					"  -c ip:port     - address of another cheatcoin full node to connect\n"
					"  -d             - run as daemon (default is interactive mode)\n"
					"  -h             - print this help\n"
					"  -i             - run as interactive terminal for daemon running in this folder\n"
					"  -l             - output non zero balances of all accounts\n"
					"  -m N           - use N CPU mining threads (default is 0)\n"
					"  -p ip:port     - public address of this node\n"
				    "  -P ip:port:CFG - run the pool, bind to ip:port, CFG is miners:fee:reward:direct:maxip:fund\n"
				    "                     miners - maximum allowed number of miners,\n"
				    "                     fee - pool fee in percent,\n"
				    "                     reward - reward to miner who got a block in percent,\n"
				    "                     direct - reward to miners participated in earned block in percent,\n"
				    "                     maxip - maximum allowed number of miners connected from single ip,\n"
				    "                     fund - community fund fee in percent\n"
				    "  -r             - load local blocks and wait for 'run' command to continue\n"
					"  -s ip:port     - address of this node to bind to\n"
					"  -t             - connect to test net (default is main net)\n"
					"  -v N           - set loglevel to N\n"
				, argv[0]);
				return 0;
		    case 'i':
			    return terminal();
			case 'l':
				return out_balances();
			case 'm':
				if (++i < argc) {
					sscanf(argv[i], "%d", &n_mining_threads);
					if (n_mining_threads < 0) n_mining_threads = 0;
				}
				break;
			case 'p':
			    if (++i < argc)
					pubaddr = argv[i];
			    break;
			case 'P':
				if (++i < argc)
					is_pool = 1, pool_arg = argv[i];
				break;
		    case 'r':
			    g_cheatcoin_run = 0;
			    break;
			case 's':
				if (++i < argc)
					bindto = argv[i];
				break;
			case 't':
				g_cheatcoin_testnet = 1;
				break;
			case 'v':
				if (++i < argc && sscanf(argv[i], "%d", &level) == 1)
					cheatcoin_set_log_level(level);
				else { printf("Illevel use of option -v\n"); return -1; }
				break;
			default:
				goto help;
		} else if (strchr(argv[i], ':')) {
			is_miner = 1;
			pool_arg = argv[i];
		}
	}
	if (is_miner && (is_pool || pubaddr || bindto || n_addrports)) {
		printf("Miner can't be a pool or have directly connected to the cheatcoin network.\n");
		return -1;
	}
	g_is_miner = is_miner;
	g_is_pool = is_pool;
	if (pubaddr && !bindto) {
		char str[64], *p = strchr(pubaddr, ':');
		if (p) { sprintf(str, "0.0.0.0%s", p); bindto = strdup(str); }
	}
	memset(&g_cheatcoin_stats, 0, sizeof(g_cheatcoin_stats));
	memset(&g_cheatcoin_extstats, 0, sizeof(g_cheatcoin_extstats));

	cheatcoin_mess("Starting %s, version %s", g_progname, CHEATCOIN_VERSION);
	cheatcoin_mess("Starting synchonization engine...");
	if (cheatcoin_sync_init()) return -1;
	cheatcoin_mess("Starting dnet transport...");
	printf("Transport module: ");
	if (cheatcoin_transport_start(transport_flags, bindto, n_addrports, addrports)) return -1;
	cheatcoin_mess("Initializing log system...");
	if (cheatcoin_log_init()) return -1;
	if (!is_miner) {
		cheatcoin_mess("Reading hosts database...");
		if (cheatcoin_netdb_init(pubaddr, n_addrports, addrports)) return -1;
	}
	cheatcoin_mess("Initializing cryptography...");
	if (cheatcoin_crypt_init(1)) return -1;
	cheatcoin_mess("Reading wallet...");
	if (cheatcoin_wallet_init()) return -1;
	cheatcoin_mess("Initializing addresses...");
	if (cheatcoin_address_init()) return -1;
	cheatcoin_mess("Starting blocks engine...");
	if (cheatcoin_blocks_start((is_miner ? ~n_mining_threads : n_mining_threads), !!miner_address)) return -1;
	cheatcoin_mess("Starting pool engine...");
	if (cheatcoin_pool_start(is_pool, pool_arg, miner_address)) return -1;
#ifndef CHEATCOINWALLET
	cheatcoin_mess("Starting terminal server...");
	if (pthread_create(&th, 0, &terminal_thread, 0)) return -1;

	if (!(transport_flags & CHEATCOIN_DAEMON)) printf("Type command, help for example.\n");
	for(;;) {
		if (transport_flags & CHEATCOIN_DAEMON) sleep(100);
		else {
			char cmd[CHEATCOIN_COMMAND_MAX];
			printf("%s> ", g_progname); fflush(stdout);
			fgets(cmd, CHEATCOIN_COMMAND_MAX, stdin);
			if (cheatcoin_command(cmd, stdout) < 0) break;
		}
	}
#endif
	return 0;
}

int cheatcoin_set_password_callback(int (*callback)(const char *prompt, char *buf, unsigned size)) {
	return cheatcoin_user_crypt_action((uint32_t *)(void *)callback, 0, 0, 6);
}

int cheatcoin_show_state(cheatcoin_hash_t hash) {
	char balance[64], address[64], state[256];
	if (!g_cheatcoin_show_state) return -1;
	if (g_cheatcoin_state < CHEATCOIN_STATE_XFER) strcpy(balance, "Not ready");
	else sprintf(balance, "%.9Lf", amount2cheatcoins(cheatcoin_get_balance(0)));
	if (!hash) strcpy(address, "Not ready");
	else strcpy(address, cheatcoin_hash2address(hash));
	strcpy(state, get_state());
	return (*g_cheatcoin_show_state)(state, balance, address);
}
