/* dnet: commands; T11.261-T13.808; $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "dnet_command.h"
#include "dnet_database.h"
#include "dnet_packet.h"
#include "dnet_stream.h"
#include "dnet_files.h"
#include "dnet_main.h"
#include "../utils/utils.h"

#define HISTORY_FILE "dnet_history.txt"

static char *dnet_strtok_r(char *str, const char *delim, char **lasts) {
	char *res0 = 0, *res;
	int len = 0;
	while ((res = strtok_r(str, delim, lasts))) {
		str = 0;
		if (res0) {
			char *res1 = res0 + len;
			while (res1 < res) *res1++ = ' ';
			res = res0;
			res0 = 0;
		}
		if (*res != '"') break;
		len = strlen(res);
		if (len > 1 && res[len - 1] == '"') {
			res[len - 1] = 0;
			res++;
			break;
		}
		res0 = res;
	}
	return res ? res : res0;
}

int dnet_command(const char *in, struct dnet_output *out) {
    char *cmd, *lasts, inbuf[DNET_COMMAND_MAX];
    struct dnet_host *host;
	FILE *f;
	int len;
	strcpy(inbuf, in);
	cmd = inbuf;
begin:
	while (*cmd && isspace(*cmd)) ++cmd;
	len = strlen(cmd);
	while (len && isspace(cmd[len - 1])) cmd[--len] = 0;
	if (!*cmd) return 0;
	f = xdag_open_file(HISTORY_FILE, "a");
	if (f) {
		fprintf(f, "%s\n", cmd);
		xdag_close_file(f);
	}
	cmd = strtok_r(cmd, " \t\r\n", &lasts);
	if (!cmd) {
		return 0;
	} else if (!dnet_limited_version && !strcmp(cmd, "conn")) {
        dnet_print_connections(out);
	} else if (!dnet_limited_version && !strcmp(cmd, "connect")) {
		char *host  = dnet_strtok_r(0, " \t\r\n", &lasts);
		if (!host) dnet_printf(out, "dnet: host is not given\n");
		else {
			struct dnet_thread *thread = malloc(sizeof(struct dnet_thread) + strlen(host) + 1);
			int res = 1;
			if (thread) {
				strcpy((char *)(thread + 1), host);
				thread->arg = (char *)(thread + 1);
				thread->conn.socket = -1;
				thread->type = DNET_THREAD_CLIENT;
				res = dnet_thread_create(thread);
				if (res) res = res << 4 | 2;
			}
			if (res) dnet_printf(out, "dnet: can't connect (error %X)\n", res);
		}
	} else if (!strcmp(cmd, "connlimit")) {
		char *str = strtok_r(0, " \t\r\n", &lasts);
		if (!str) dnet_printf(out, "Connection limit: %d\n", g_conn_limit);
		else if (sscanf(str, "%d", &g_conn_limit) != 1)
			dnet_printf(out, "dnet: illegal parameter of the connlimit command\n");
	} else if (!strcmp(cmd, "copy")) {
		char *from, *to;
		int err = 0;
		if (!(from = dnet_strtok_r(0, " \t\r\n", &lasts)) || !(to = dnet_strtok_r(0, " \t\r\n", &lasts))
				|| (err = dnet_file_command(from, to, dnet_strtok_r(0, " \t\r\n", &lasts), out)))
			dnet_printf(out, "dnet: illegal parameters of copy command (error %X)\n", err);
	} else if (!strcmp(cmd, "help") || !strcmp(cmd , "?")) {
		dnet_printf(out,
			"Commands:\n"
		);
		if (!dnet_limited_version) dnet_printf(out,
			"  conn                          - list connections\n"
		    "  connect ip:port               - connect to this host\n"
			"  connlimit [<N>]               - print of set the inbound connection limit\n"
		);
		dnet_printf(out,
		    "  help, ?                       - print this help\n"
			"  history [<N>]                 - print last N commands (20 by default)\n"
			"  host [<host>]                 - print brief information about the host (the current host by default)\n"
		);
		if (!dnet_limited_version) dnet_printf(out,
			"  hosts [-a|-d|-h]              - list active [all|daily|hourly] hosts\n"
		);
	} else if (!strcmp(cmd, "history")) {
		char *arg;
		int N = 20;
		if ((arg = strtok_r(0, " \t\r\n", &lasts))) sscanf(arg, "%d", &N);
		sprintf(inbuf, "tail -n %u " HISTORY_FILE, N);
		cmd = inbuf; goto begin;
	} else if (!strcmp(cmd, "host")) {
		char *name;
		if ((name = strtok_r(0, " \t\r\n", &lasts))) {
			host = dnet_get_host_by_name(name);
			if (!host) {
				dnet_printf(out, "dnet: unknown host\n");
				return 1;
			}
		} else host = dnet_get_self_host();
		dnet_print_host_brief(host, out);
	} else if (!dnet_limited_version && !strcmp(cmd, "hosts")) {
		char *arg;
		if ((arg = strtok_r(0, " \t\r\n", &lasts))) {
			if (!strcmp(arg, "-a")) dnet_print_hosts(out, LONG_MAX);
			else if (!strcmp(arg, "-d")) dnet_print_hosts(out, 3600*24);
			else if (!strcmp(arg, "-h")) dnet_print_hosts(out, 3600);
			else dnet_printf(out, "dnet: illegal parameter\n");
		} else dnet_print_hosts(out, DNET_ACTIVE_PERIOD);
	}
	return 0;
}

int dnet_execute_command(const char *cmd, void *fileout) {
	struct dnet_output out;
	memset(&out, 0, sizeof(out));
	out.f = (FILE *)fileout;
	return dnet_command(cmd, &out);
}
