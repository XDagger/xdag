/* cheatcoin main, T13.654-T13.726 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
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

#define CHEATCOIN_COMMAND_MAX	0x1000
#define FIFO_IN					"cheatcoin_cmd.dat"
#define FIFO_OUT				"cheatcoin_res.dat"
#define XFER_MAX_IN				11

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

int g_cheatcoin_testnet = 0;
struct cheatcoin_stats g_cheatcoin_stats;
struct cheatcoin_ext_stats g_cheatcoin_extstats;

static long double amount2cheatcoins(cheatcoin_amount_t amount) {
	long double res = 0, d = 1;
	int i;
	for (i = 0; amount && i < 64; ++i, amount >>= 1) {
		if (amount & 1) res += d;
		if (i < 32) res /= 2; else d *= 2;
	}
	return res;
}

static cheatcoin_amount_t cheatcoins2amount(const char *str) {
	long double sum, flr;
	cheatcoin_amount_t res;
	int i;
	if (sscanf(str, "%Lf", &sum) != 1 || sum < 0) return 0;
	flr = floorl(sum);
	res = (cheatcoin_amount_t)flr << 32;
	sum -= flr;
	for (i = 31; i >= 0; --i) {
		sum *= 2;
		if (sum >= 1) res |= 1l << i, sum--;
	}
	return res;
}

static int account_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, int n_our_key) {
	struct account_callback_data *d = (struct account_callback_data *)data;
	if (!d->count--) return -1;
	fprintf(d->out, "%s %20.9Lf  key %d\n", cheatcoin_hash2address(hash), amount2cheatcoins(amount), n_our_key);
	return 0;
}

static int make_block(struct xfer_callback_data *d) {
	int res;
	if (d->nfields != XFER_MAX_IN) memcpy(d->fields + d->nfields, d->fields + XFER_MAX_IN, sizeof(cheatcoin_hashlow_t));
	d->fields[d->nfields].amount = d->todo;
	res = cheatcoin_create_block(d->fields, d->nfields, 1, 0, 0);
	if (res) {
		cheatcoin_err("FAILED: to %s xfer %.9Lf cheatcoins, error %d",
				cheatcoin_hash2address(d->fields[d->nfields].hash), amount2cheatcoins(d->todo), res);
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

static int xfer_callback(void *data, cheatcoin_hash_t hash, cheatcoin_amount_t amount, int n_our_key) {
	struct xfer_callback_data *d = (struct xfer_callback_data *)data;
	cheatcoin_amount_t todo = d->remains;
	int i;
	if (!amount) return -1;
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
	cheatcoin_mess("Xfer  : from %s to %s xfer %.9Lf cheatcoins",
			cheatcoin_hash2address(hash), cheatcoin_hash2address(d->fields[XFER_MAX_IN].hash), amount2cheatcoins(todo));
	if (!d->remains || Nfields(d) == CHEATCOIN_BLOCK_FIELDS) {
		if (make_block(d)) return -1;
		if (!d->remains) return 1;
	}
	return 0;
}

static int cheatcoin_command(char *cmd, FILE *out) {
	char *lasts;
	cmd = strtok_r(cmd, " \t\r\n", &lasts);
	if (!cmd) return 0;
	else if (!strcmp(cmd, "account")) {
		struct account_callback_data d;
		d.out = out;
		d.count = 20;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (cmd) sscanf(cmd, "%d", &d.count);
		cheatcoin_traverse_our_blocks(&d, &account_callback);
	} else if (!strcmp(cmd, "balance")) {
		cheatcoin_hash_t hash;
		cheatcoin_amount_t balance;
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (cmd) {
			cheatcoin_address2hash(cmd, hash);
			balance = cheatcoin_get_balance(hash);
		} else balance = cheatcoin_get_balance(0);
		fprintf(out, "Balance: %.9Lf cheatcoins\n", amount2cheatcoins(balance));
	} else if (!strcmp(cmd, "exit")) return -1;
	else if (!strcmp(cmd, "help")) {
		fprintf(out, "Commands:\n"
			"  account [N] - print first N (20 by default) our addresses with their amounts\n"
			"  balance [A] - print balance of the address A or total balance for all our addresses\n"
		    "  exit        - exit this program (not the daemon)\n"
			"  help        - print this help\n"
		    "  keygen      - generate new private/public key pair and set it by default\n"
		    "  level [N]   - print level of logging or set it to N (0 - nothing, ..., 9 - all)\n"
		    "  net command - run transport layer command, try 'net help'\n"
		    "  stats       - print statistics for loaded and all known blocks\n"
			"  terminate   - terminate both daemon and this program\n"
			"  xfer S A    - transfer S our cheatcoins to the address A\n"
		);
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
	} else if (!strcmp(cmd, "net")) {
		char netcmd[4096];
		*netcmd = 0;
		while ((cmd = strtok_r(0, " \t\r\n", &lasts))) { strcat(netcmd, cmd); strcat(netcmd, " "); }
		cheatcoin_net_command(netcmd, out);
	} else if (!strcmp(cmd, "stats")) {
		fprintf(out, "Statistics for ours and maximum known parameters:\n"
			"            hosts: %u of %u\n"
			"           blocks: %lu of %lu\n"
			"      main blocks: %lu of %lu\n"
			"    orphan blocks: %lu\n"
			" chain difficulty: %lx%016lx of %lx%016lx\n"
			"cheatcoins supply: %.9Lf of %.9Lf\n",
			g_cheatcoin_stats.nhosts, g_cheatcoin_stats.total_nhosts,
			g_cheatcoin_stats.nblocks, g_cheatcoin_stats.total_nblocks,
			g_cheatcoin_stats.nmain, g_cheatcoin_stats.total_nmain,
			g_cheatcoin_extstats.nnoref,
			(unsigned long)(g_cheatcoin_stats.difficulty >> 64), (unsigned long)g_cheatcoin_stats.difficulty,
			(unsigned long)(g_cheatcoin_stats.max_difficulty >> 64), (unsigned long)g_cheatcoin_stats.max_difficulty,
			amount2cheatcoins(cheatcoin_get_supply(g_cheatcoin_stats.nmain)),
			amount2cheatcoins(cheatcoin_get_supply(g_cheatcoin_stats.total_nmain))
		);
	} else if (!strcmp(cmd, "terminate")) {
		cheatcoin_wallet_finish();
		cheatcoin_netdb_finish();
		cheatcoin_storage_finish();
		return -1;
	} else if (!strcmp(cmd, "xfer")) {
		struct xfer_callback_data xfer;
		memset(&xfer, 0, sizeof(xfer));
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) { fprintf(out, "Xfer: amount not given.\n"); return 1; }
		xfer.remains = cheatcoins2amount(cmd);
		if (!xfer.remains) { fprintf(out, "Xfer: nothing to transfer.\n"); return 1; }
		if (xfer.remains > cheatcoin_get_balance(0)) { fprintf(out, "Xfer: balance too small.\n"); return 1; }
		cmd = strtok_r(0, " \t\r\n", &lasts);
		if (!cmd) { fprintf(out, "Xfer: destination address not given.\n"); return 1; }
		cheatcoin_address2hash(cmd, xfer.fields[XFER_MAX_IN].hash);
		cheatcoin_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
		xfer.outsig = 1;
		cheatcoin_traverse_our_blocks(&xfer, &xfer_callback);
		fprintf(out, "Xfer: transferred %.9Lf cheatcoins to the address %s, see log for details.\n",
				amount2cheatcoins(xfer.done), cheatcoin_hash2address(xfer.fields[XFER_MAX_IN].hash));
	}
	return 0;
}

static int terminal(void) {
	char cmd[CHEATCOIN_COMMAND_MAX], cmd2[CHEATCOIN_COMMAND_MAX], *ptr, *lasts;
	int fd;
	int c = 0;
	while(1) {
		printf("cheatcoin> "); fflush(stdout);
		fgets(cmd, CHEATCOIN_COMMAND_MAX, stdin);
		strcpy(cmd2, cmd);
		ptr = strtok_r(cmd2, " \t\r\n", &lasts);
		if (!ptr) continue;
		if (!strcmp(ptr, "exit")) break;
		fd = open(FIFO_IN, O_WRONLY);
		if (fd < 0) { printf("Can't open pipe.\n"); continue; }
		write(fd, cmd, strlen(cmd) + 1);
		close(fd);
		fd = open(FIFO_OUT, O_RDONLY);
		if (fd < 0) { printf("Can't open pipe.\n"); continue; }
		while (read(fd, &c, 1) == 1 && c) putchar(c);
		close(fd);
		if (!strcmp(ptr, "terminate")) break;
	}
	return 0;
}

static void *terminal_thread(void *arg) {
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
	return 0;
}

int main(int argc, char **argv) {
	const char *addrports[256], *bindto = 0, *pubaddr = 0;
	int transport_flags = 0, n_addrports = 0, n_maining_threads = 0, i;
	pthread_t th;
	signal(SIGPIPE, SIG_IGN);
	printf("Cheatcoin full node client/server, version %s.\n", CHEATCOIN_VERSION);
	if (argc <= 1) goto help;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] && !argv[i][2]) switch(argv[i][1]) {
			case 'c':
				if (++i < argc && n_addrports < 256)
					addrports[n_addrports++] = argv[i];
				break;
			case 'd':
				transport_flags |= CHEATCOIN_DAEMON;
				break;
			case 'h':
			help:
				printf("Usage: %s flags\n"
					"Flags:\n"
					"  -c ip:port  - address of another cheatcoin full node to connect\n"
					"  -d          - run as daemon (default is interactive mode)\n"
					"  -h          - print this help\n"
				    "  -i          - run as interactive terminal for daemon running in this folder\n"
				    "  -m N        - use N CPU maining threads (default is 0)\n"
				    "  -p ip:port  - public address of this node\n"
				    "  -s ip:port  - address of this node to bind to\n"
					"  -t          - connect to test net (default is main net)\n"
				, argv[0]);
				return 0;
		    case 'i':
			    return terminal();
		    case 'm':
				if (++i < argc)
					sscanf(argv[i], "%d", &n_maining_threads);
				break;
			case 'p':
			    if (++i < argc)
					pubaddr = argv[i];
			    break;
		    case 's':
				if (++i < argc)
					bindto = argv[i];
				break;
			case 't':
				g_cheatcoin_testnet = 1;
				break;
			default:
				goto help;
		}
	}
	if (pubaddr && !bindto) {
		char str[64], *p = strchr(pubaddr, ':');
		if (p) { sprintf(str, "0.0.0.0%s", p); bindto = strdup(str); }
	}
	memset(&g_cheatcoin_stats, 0, sizeof(g_cheatcoin_stats));
	memset(&g_cheatcoin_extstats, 0, sizeof(g_cheatcoin_extstats));

	cheatcoin_mess("Starting cheatcoin, version %s", CHEATCOIN_VERSION);
	cheatcoin_mess("Starting dnet transport...");
	printf("Transport module: ");
	if (cheatcoin_transport_start(transport_flags, bindto, n_addrports, addrports)) return -1;
	cheatcoin_mess("Reading hosts database...");
	if (cheatcoin_netdb_init(pubaddr, n_addrports, addrports)) return -1;
	cheatcoin_mess("Initializing cryptography...");
	if (cheatcoin_crypt_init()) return -1;
	cheatcoin_mess("Reading wallet...");
	if (cheatcoin_wallet_init()) return -1;
	cheatcoin_mess("Initializing addresses...");
	if (cheatcoin_address_init()) return -1;
	cheatcoin_mess("Starting blocks engine...");
	if (cheatcoin_blocks_start(n_maining_threads)) return -1;
	cheatcoin_mess("Starting terminal server...");
	if (pthread_create(&th, 0, &terminal_thread, 0)) return -1;

	if (!(transport_flags & CHEATCOIN_DAEMON)) printf("Type command, help for example.\n");
	for(;;) {
		if (transport_flags & CHEATCOIN_DAEMON) sleep(100);
		else {
			char cmd[CHEATCOIN_COMMAND_MAX];
			printf("cheatcoin> "); fflush(stdout);
			fgets(cmd, CHEATCOIN_COMMAND_MAX, stdin);
			if (cheatcoin_command(cmd, stdout) < 0) break;
		}
	}

	return 0;
}
