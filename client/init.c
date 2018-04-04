/* cheatcoin main, T13.654-T13.895 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#endif
#include "system.h"
#include "address.h"
#include "block.h"
#include "crypt.h"
#include "log.h"
#include "transport.h"
#include "version.h"
#include "wallet.h"
#include "netdb.h"
#include "init.h"
#include "sync.h"
#include "pool.h"
#include "commands.h"
#include "terminal.h"
#include "../utils/utils.h"

char *g_coinname, *g_progname;
#define coinname   g_coinname

int g_xdag_state = XDAG_STATE_INIT;
int g_xdag_testnet = 0;
int g_is_miner = 0;
static int g_is_pool = 0;
int g_xdag_run = 0;
time_t g_xdag_xfer_last = 0;
struct xdag_stats g_xdag_stats;
struct xdag_ext_stats g_xdag_extstats;
int(*g_xdag_show_state)(const char *state, const char *balance, const char *address) = 0;

void printUsage(char* appName);

int xdag_init(int argc, char **argv, int isGui)
{
    xdag_init_path(argv[0]);
    
	const char *addrports[256], *bindto = 0, *pubaddr = 0, *pool_arg = 0, *miner_address = 0;
	char *ptr;
	int transport_flags = 0, n_addrports = 0, n_mining_threads = 0, is_pool = 0, is_miner = 0, level;
#if !defined(_WIN32) && !defined(_WIN64)
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGWINCH, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
#endif
	g_progname = strdup(argv[0]);
	while ((ptr = strchr(g_progname, '/')) || (ptr = strchr(g_progname, '\\'))) g_progname = ptr + 1;
	if ((ptr = strchr(g_progname, '.'))) *ptr = 0;
	for (ptr = g_progname; *ptr; ptr++) *ptr = tolower((unsigned char)*ptr);
	coinname = strdup(g_progname);
	for (ptr = coinname; *ptr; ptr++) *ptr = toupper((unsigned char)*ptr);

	if (!isGui) {
		printf("%s client/server, version %s.\n", g_progname, XDAG_VERSION);
	}

	g_xdag_run = 1;
	xdag_show_state(0);

	if (argc <= 1) {
		printUsage(argv[0]);
		return 0;
	}

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-' || !argv[i][1] || argv[i][2]) {
			if (strchr(argv[i], ':')) {
				is_miner = 1;
				pool_arg = argv[i];
			}
			continue;
		}

		switch (argv[i][1]) {
		case 'a':
			if (++i < argc) miner_address = argv[i];
			break;
		case 'c':
			if (++i < argc && n_addrports < 256)
				addrports[n_addrports++] = argv[i];
			break;
		case 'd':
#if !defined(_WIN32) && !defined(_WIN64)
			transport_flags |= XDAG_DAEMON;
#endif
			break;
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
			if (++i < argc) {
				is_pool = 1;
				pool_arg = argv[i];
			}
			break;
		case 'r':
			g_xdag_run = 0;
			break;
		case 's':
			if (++i < argc)
				bindto = argv[i];
			break;
		case 't':
			g_xdag_testnet = 1;
			break;
		case 'v':
			if (++i < argc && sscanf(argv[i], "%d", &level) == 1) {
				xdag_set_log_level(level);
			} else {
				printf("Illevel use of option -v\n");
				return -1;
			}
			break;
		default:
			printUsage(argv[0]);
			return 0;
		}
	}
	if (is_miner && (is_pool || pubaddr || bindto || n_addrports)) {
		printf("Miner can't be a pool or have directly connected to the xdag network.\n");
		return -1;
	}
	g_is_miner = is_miner;
	g_is_pool = is_pool;
	if (pubaddr && !bindto) {
		char str[64], *p = strchr(pubaddr, ':');
		if (p) {
			sprintf(str, "0.0.0.0%s", p);
			bindto = strdup(str);
		}
	}
	memset(&g_xdag_stats, 0, sizeof(g_xdag_stats));
	memset(&g_xdag_extstats, 0, sizeof(g_xdag_extstats));

	xdag_mess("Starting %s, version %s", g_progname, XDAG_VERSION);
	xdag_mess("Starting synchonization engine...");
	if (xdag_sync_init()) return -1;
	xdag_mess("Starting dnet transport...");
	printf("Transport module: ");
	if (xdag_transport_start(transport_flags, bindto, n_addrports, addrports)) return -1;
	xdag_mess("Initializing log system...");
	if (xdag_log_init()) return -1;
	if (!is_miner) {
		xdag_mess("Reading hosts database...");
		if (xdag_netdb_init(pubaddr, n_addrports, addrports)) return -1;
	}
	xdag_mess("Initializing cryptography...");
	if (xdag_crypt_init(1)) return -1;
	xdag_mess("Reading wallet...");
	if (xdag_wallet_init()) return -1;
	xdag_mess("Initializing addresses...");
	if (xdag_address_init()) return -1;
	xdag_mess("Starting blocks engine...");
	if (xdag_blocks_start((is_miner ? ~n_mining_threads : n_mining_threads), !!miner_address)) return -1;
	xdag_mess("Starting pool engine...");
	if (xdag_pool_start(is_pool, pool_arg, miner_address)) return -1;

	if (!isGui) {
		if (is_pool || (transport_flags & XDAG_DAEMON) > 0) {
			xdag_mess("Starting terminal server...");
			pthread_t th;
			if (pthread_create(&th, 0, &terminal_thread, 0)) {
				return -1;
			}
		}

		startCommandProcessing(transport_flags);
	}

	return 0;
}

int xdag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size))
{
    return xdag_user_crypt_action((uint32_t *)(void *)callback, 0, 0, 6);
}

void printUsage(char* appName)
{
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
		, appName);
}
