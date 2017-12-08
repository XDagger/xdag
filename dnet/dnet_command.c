/* dnet: commands; T11.261-T13.714; $DVS:time$ */

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
	f = fopen(HISTORY_FILE, "a");
	if (f) {
		fprintf(f, "%s\n", cmd);
		fclose(f);
	}
	cmd = strtok_r(cmd, " \t\r\n", &lasts);
	if (!cmd) {
		return 0;
#ifndef CHEATCOIN
	} else if (!strcmp(cmd, "close")) {
		char *idstr; uint16_t id;
		if ((idstr = strtok_r(0, " \t\r\n", &lasts)) && sscanf(idstr, "%hx", &id) == 1) {
			if (!dnet_finish_stream(id)) dnet_printf(out, "dnet: can't find stream %hx\n", id);
		} else dnet_printf(out, "dnet: illegal parameters of close command\n");
#endif
	} else if (!dnet_limited_version && !strcmp(cmd, "conn")) {
        dnet_print_connections(out);
	} else if (!dnet_limited_version && !strcmp(cmd, "connect")) {
		char *host  = dnet_strtok_r(0, " \t\r\n", &lasts);
		if (!host) dnet_printf(out, "dnet: host is not given\n");
		else {
			struct dnet_thread *thread = malloc(sizeof(struct dnet_thread));
			int res = 1;
			if (thread) {
				thread->arg = strdup(host);
				thread->conn.socket = -1;
				thread->type = DNET_THREAD_CLIENT;
				res = dnet_thread_create(thread);
				if (res) res = res << 4 | 2;
			}
			if (res) dnet_printf(out, "dnet: can't connect (error %X)\n", res);
		}
	} else if (!strcmp(cmd, "copy")) {
		char *from, *to;
		int err = 0;
		if (!(from = dnet_strtok_r(0, " \t\r\n", &lasts)) || !(to = dnet_strtok_r(0, " \t\r\n", &lasts))
				|| (err = dnet_file_command(from, to, dnet_strtok_r(0, " \t\r\n", &lasts), out)))
			dnet_printf(out, "dnet: illegal parameters of copy command (error %X)\n", err);
#ifndef CHEATCOIN
	} else if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) {
        return -1;
#endif
	} else if (!strcmp(cmd, "help") || !strcmp(cmd , "?")) {
		dnet_printf(out,
			"Commands:\n"
#ifndef CHEATCOIN
			"  close <id>                    - close stream\n"
#endif
		);
		if (!dnet_limited_version) dnet_printf(out,
			"  conn                          - list connections\n"
		    "  connect ip:port               - connect to this host\n"
		);
		dnet_printf(out,
#ifndef CHEATCOIN
		    "  copy [h:]f [H:]F [p,s,t]      - copy file/catalog f from host h to file/catalog F on host H;\n"
			"                                  p - start position, s - max speed bytes/sec, t - seconds without speed restriction\n"
		    "  exit, quit                    - exit the program\n"
#endif
		    "  help, ?                       - print this help\n"
			"  history [<N>]                 - print last N commands (20 by default)\n"
			"  host [<host>]                 - print brief information about the host (the current host by default)\n"
		);
		if (!dnet_limited_version) dnet_printf(out,
			"  hosts [-a|-d|-h]              - list active [all|daily|hourly] hosts\n"
		);
#ifndef CHEATCOIN
		dnet_printf(out,
			"  log <file> <text>             - append text to the end of file\n"
			"  rename <old> <new>            - rename the host\n"
			"  streams                       - list active streams\n"
			"  tcp <port> <host> <[ip:]port> - forward local tcp port <port> to remote destination [ip:]port on the host\n"
			"  trust <host>                  - assume host as trusted\n"
			"  tunnel <n> <host> <m>         - create tunnel from local interface tap<n> to interface tap<m> on the host\n"
			"  udp <port> <host> <[ip:]port> - forward local udp port <port> to remote destination [ip:]port on the host\n"
			"  untrust <host>                - assume host as untrusted\n"
			"  <host>                        - login on the host \n"
			"  <host> <command>              - execute shell command on the host\n"
			"  <command>                     - execute shell command\n"
        );
#endif
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
#ifndef CHEATCOIN
	} else if (!strcmp(cmd, "log")) {
        char *fname, *text;
        FILE *f;
        if ((fname = strtok_r(0, " \t\r\n", &lasts)) && (text = strtok_r(0, "", &lasts)) && *text
                && (f = fopen(fname, "a"))) {
            fprintf(f, "%s", text);
            fclose(f);
        } else {
            dnet_printf(out, "dnet: illegal parameters of log command\n");
            return 1;
        }
    } else if (!strcmp(cmd, "rename")) {
        char *newname, *oldname;
        int len;
		if (!(oldname = strtok_r(0, " \t\r\n", &lasts)) || !(newname = strtok_r(0, " \t\r\n", &lasts))
				|| !(host = dnet_get_host_by_name(oldname)) || (dnet_limited_version && host != dnet_get_self_host())
				|| (len = strlen(newname)) > DNET_HOST_NAME_MAX || dnet_set_host_name(host, newname, len)) {
            dnet_printf(out, "dnet: illegal name\n");
            return 1;
        }
    } else if (!strcmp(cmd, "streams")) {
		dnet_print_streams(out);
	} else if (!strcmp(cmd, "tcp") || !strcmp(cmd, "udp")) {
		char *ns_port, *host_to, *ns_ipport;
		unsigned port_from, port_to, ip[4];
		int proto = (strcmp(cmd, "udp") ? DNET_TCP : DNET_UDP);
		if ((ns_port = strtok_r(0, " \t\r\n", &lasts)) && sscanf(ns_port, "%u", &port_from) == 1 && !(port_from & ~0xffff)
				&& (host_to = strtok_r(0, " \t\r\n", &lasts)) && (host = dnet_get_host_by_name(host_to))
				&& (ns_ipport = strtok_r(0, " \t\r\n", &lasts))
				&& (sscanf(ns_ipport, "%u.%u.%u.%u:%u", ip, ip + 1, ip + 2, ip + 3, &port_to) == 5
				|| ((ip[0] = 127, ip[1] = ip[2] = 0, ip[3] = 1), sscanf(ns_ipport, "%u", &port_to) == 1))
				&& !((ip[0] | ip[1] | ip[2] | ip[3]) & ~0xff) && !(port_to & ~0xffff)) {
			dnet_stream_forward(host->crc32, proto, port_from, ip[0] | ip[1] << 8 | ip[2] << 16 | ip[3] << 24, port_to);
		} else {
			dnet_printf(out, "dnet: illegal %s forwarding parameters\n", (proto == DNET_TCP ? "tcp" : "udp"));
			return 1;
		}
	} else if (!strcmp(cmd, "trust")) {
		char *name;
		if (!(name = strtok_r(0, " \t\r\n", &lasts)) || !(host = dnet_get_host_by_name(name)) || dnet_trust_host(host)) {
			dnet_printf(out, "dnet: illegal host\n");
			return 1;
		}
	} else if (!strcmp(cmd, "tunnel")) {
		char *ns_from, *host_to, *ns_to;
		int n_from, n_to;
		if ((ns_from = strtok_r(0, " \t\r\n", &lasts)) && sscanf(ns_from, "%d", &n_from) == 1
				&& (host_to = strtok_r(0, " \t\r\n", &lasts)) && (host = dnet_get_host_by_name(host_to))
				&& (ns_to = strtok_r(0, " \t\r\n", &lasts)) && sscanf(ns_to, "%d", &n_to) == 1) {
			dnet_stream_create_tunnel(host->crc32, n_from, n_to, 0);
		} else {
			dnet_printf(out, "dnet: illegal tunnel parameters\n");
			return 1;
		}
	} else if (!strcmp(cmd, "untrust")) {
		char *name;
		int res;
		if (!(res = 1, name = strtok_r(0, " \t\r\n", &lasts)) || !(res = 2, host = dnet_get_host_by_name(name))
				|| (res = dnet_untrust_host(host))) {
			dnet_printf(out, "dnet: illegal host, error %d\n", res);
			return 1;
		}
	} else if ((host = dnet_get_host_by_name(cmd))) {
        cmd = strtok_r(0, "", &lasts);
        if (!cmd) {
            int err;
        stream:
            if (host == dnet_get_self_host()) return 0;
            err = dnet_stream_main(host->crc32);
            if (err) {
                dnet_printf(out, "dnet: error %X during stream initiation\n", err);
				err = err << 4 | 2;
            }
            return err;
        }
        while (*cmd && isspace((uint8_t)*cmd)) cmd++;
        if (!*cmd) goto stream;
        if (host == dnet_get_self_host()) {
			goto begin;
        } else {
            struct dnet_packet_stream st;
            int len = strlen(cmd), err;
            if (len >= DNET_PKT_STREAM_DATA_MAX) {
                dnet_printf(out, "dnet: too long command to transfer\n");
                return 3;
            }
            st.header.type = DNET_PKT_COMMAND;
            st.header.ttl = 16;
            st.header.length = DNET_PKT_STREAM_MIN_LEN + len;
            st.crc_from = dnet_get_self_host()->crc32;
            st.crc_to = host->crc32;
            st.seq = st.ack = 0;
            dnet_generate_stream_id(&st.id);
            memcpy(st.data, cmd, len);
//	        printf("[1:%d:%*s]\n", len, len, st.data);
			err = dnet_send_command_packet(&st, out);
//	        printf("[2:%d]\n", err);
			if (err) {
                dnet_printf(out, "dnet: can't send packet, error %X\n", err);
                return err << 4 | 4;
            }
			if (out->f == stdout) sleep(1);
			else if (!out->f) {
				sleep(1);
				dnet_cancel_command(out);
			}
        }
    } else {
		char outbuf[DNET_PKT_STREAM_DATA_MAX + 1];
		FILE *f;
		int len0 = strlen(cmd);
		while (len0 < len && !cmd[len0]) cmd[len0++] = ' ';
		f = popen(cmd, "r");
		if (!f) {
			dnet_printf(out, "dnet: can't execute command: %s\n", strerror(errno));
			return 5;
        }
		while ((len = fread(outbuf, 1, DNET_PKT_STREAM_DATA_MAX, f)) > 0) {
			dnet_write(out, outbuf, len);
        }
        dnet_write(out, outbuf, 0);
        pclose(f);
#endif
	}
	return 0;
}

int dnet_execute_command(const char *cmd, void *fileout) {
	struct dnet_output out;
	memset(&out, 0, sizeof(out));
	out.f = (FILE *)fileout;
	return dnet_command(cmd, &out);
}
