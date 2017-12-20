/* dnet: streams; T11.270-T13.737; $DVS:time$ */

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#ifdef __LDuS__
#include <ldus/system/kernel.h>
#include <ldus/system/stdlib.h>
#include <../config.h>
#define MEM_LIMIT_PERCENT	LDUS_CONFIG_MEM_NET_APP
#endif
#include "dnet_packet.h"
#include "dnet_stream.h"
#include "dnet_threads.h"
#include "dnet_database.h"
#include "dnet_connection.h"
#include "dnet_files.h"
#include "dnet_tap.h"

#define DNET_FORWARD_WAIT 3 /* сколько секунд ждать соединения при проброске порта */

#ifdef QDNET
#define posix_openpt(oflag) open("/dev/ptmx", oflag)
#endif

struct dnet_send_stream_packet_data {
	struct dnet_packet_stream *st;
	struct dnet_host *host;
	struct dnet_connection *conn;
	int use_port;
	int counter;
};

int g_ctrlc_handled = 0;

static int dnet_send_stream_packet_callback(struct dnet_connection *conn, void *data) {
	struct dnet_send_stream_packet_data *d = (struct dnet_send_stream_packet_data *)data;
	if (conn != d->conn && conn->ipaddr == d->host->route_ip && (!d->use_port || conn->port == d->host->route_port)) {
		dnet_send_packet((struct dnet_packet *)d->st, conn);
		d->counter++;
	}
	return 0;
}

int dnet_send_stream_packet(struct dnet_packet_stream *st, struct dnet_connection *conn) {
	struct dnet_send_stream_packet_data data;
	data.host = dnet_get_host_by_crc(st->crc_to);
	if (!data.host) return 1;
	if (data.host == dnet_get_self_host()) return 2;
	data.st = st;
	data.conn = conn;
	data.counter = 0;
	data.use_port = 1;
	dnet_traverse_connections(&dnet_send_stream_packet_callback, &data);
	if (!data.counter) {
		if (st->header.ttl > 3) st->header.ttl = 3;
		else if (st->header.ttl <= 1) return 3;
		data.use_port = 0;
		dnet_traverse_connections(&dnet_send_stream_packet_callback, &data);
		if (!data.counter) return 4;
	}
	return 0;
}

#ifndef CHEATCOIN
static void sigint_handler(int __attribute__((unused)) sig) {
	g_ctrlc_handled = 1;
}
#endif

void *dnet_thread_stream(void *arg) {
	struct dnet_thread *t = (struct dnet_thread *)arg;
#ifndef CHEATCOIN
	struct dnet_packet_stream st;
	ssize_t data_len;
	char *slavedevice;
	void (*old_handler)(int) = SIG_DFL;
#ifdef __LDuS__
	FILE *pipe;
#endif
	t->st.creation_time = time(0);
	t->st.crc_from = dnet_get_self_host()->crc32;
	t->st.seq = t->st.ack = 0;
	if (t->st.pkt_type != DNET_PKT_FORWARDED_TCP && t->st.pkt_type != DNET_PKT_FORWARDED_UDP)
		t->st.input_tty = t->st.output_tty = -1;
	t->st.child = -1;
	st.header.type = t->st.pkt_type;
	st.header.ttl = 16;
	st.crc_from = t->st.crc_from;
	st.crc_to = t->st.crc_to;
	memcpy(&st.id, &t->st.id, sizeof(struct dnet_stream_id));
	switch (t->st.pkt_type) {
	  case DNET_PKT_SHELL_INPUT:
		{
		struct winsize ws;
		struct dnet_tty_params *tty_params = (struct dnet_tty_params *)st.data;
		ioctl(1, TIOCGWINSZ, &ws);
		tty_params->xwinsize = ws.ws_col;
		tty_params->ywinsize = ws.ws_row;
		dnet_log_printf("stream %04x: outcoming, to %08x, size %dx%d\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to,
			(int)tty_params->xwinsize, (int)tty_params->ywinsize);
		t->st.input_tty = 0;
		t->st.output_tty = 1;
		data_len = sizeof(struct dnet_tty_params);
		old_handler = signal(SIGINT, &sigint_handler);
		}
		goto send;
	  case DNET_PKT_SHELL_OUTPUT:
		dnet_log_printf("stream %04x: incoming, from %08x, size %dx%d\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to,
				(int)t->st.tty_params.xwinsize, (int)t->st.tty_params.ywinsize);
		t->st.ack += sizeof(struct dnet_tty_params);
#ifndef __LDuS__
		if ((t->st.input_tty = posix_openpt(O_RDWR)) < 0
				|| grantpt(t->st.input_tty) < 0
				|| unlockpt(t->st.input_tty) < 0
				|| !(slavedevice = ptsname(t->st.input_tty))) {
			t->st.to_exit = 1; data_len = 0; goto send;
		}
		t->st.output_tty = t->st.input_tty;
		t->st.child = fork();
		if (t->st.child < 0) {
			t->st.to_exit = 1; data_len = 0; goto send;
		}
		if (!t->st.child) {
			struct winsize ws;
			int i = 256;
			struct rlimit rlim;
			const char *shell;
			if (!getrlimit(RLIMIT_NOFILE, &rlim))
				i = rlim.rlim_cur;
			setsid();
			for (--i; i >= 0; --i) close(i); /* close all descriptors */
			i = open(slavedevice, O_RDWR);
			if (i < 0) exit(1);
			dup(i); dup(i); /* handle standard I/O */
			ws.ws_col = t->st.tty_params.xwinsize;
			ws.ws_row = t->st.tty_params.ywinsize;
			ioctl(1, TIOCSWINSZ, &ws);
			shell = getenv("SHELL");
#ifdef QDNET
			if (!shell) shell = "/system/bin/sh";
#else
			if (!shell) shell = "/bin/bash";
#endif
			execl(shell, shell, (char *)0);
			exit(2);
		} else {
			dnet_log_printf("stream %04x: pid %d, device %s\n", t->st.id.id[0] & 0xFFFF, (int)t->st.child, slavedevice);
		}
#else
		{
		char input[64], output[64];
		sprintf(input, "/tmp/dnet_shell_input.%04x", t->st.id.id[0] & 0xFFFF);
		sprintf(output, "/tmp/dnet_shell_output.%04x", t->st.id.id[0] & 0xFFFF);
		pipe = fopen(input, "w");
		if (!pipe) {
			dnet_log_printf("stream %04x: error %x\n", t->st.id.id[0] & 0xFFFF, ldus_errno << 4 | 1);
			t->st.to_exit = 1; data_len = 0; goto send;
		}
		t->st.output_tty = fileno(pipe);
		pipe = ldus_popen_cmd("shell -i virtual/remote_profile.sh", input, output,
						  LDUS_POPEN_TERMINAL | LDUS_POPEN_WAIT_SIGNAL | LDUS_POPEN_STDIN_GETC
						  | LDUS_POPEN_STDIN_GETC_EXT | LDUS_POPEN_STDOUT_FILE |  LDUS_POPEN_STDERR_FILE
						  | LDUS_POPEN_MEMORY_LIMIT * ((ldus_get_total_memory() * MEM_LIMIT_PERCENT / 100) >> 20)
						  | LDUS_POPEN_TERMINAL_X * t->st.tty_params.xwinsize
						  | LDUS_POPEN_TERMINAL_Y * t->st.tty_params.ywinsize);
		if (!pipe) {
			dnet_log_printf("stream %04x: error %x\n", t->st.id.id[0] & 0xFFFF, ldus_errno << 4 | 2);
			t->st.to_exit = 1; data_len = 0; goto send;
		}
		t->st.input_tty = fileno(pipe);
		t->st.child = pipe->pid;
		dnet_log_printf("stream %04x: pid %d\n", t->st.id.id[0] & 0xFFFF, (int)t->st.child);
		}
#endif
		break;
	  case DNET_PKT_TUNNELED_MSG:
		{
		int tun_fd = dnet_tap_open(t->st.tap_from);  /* open tap device */

		if (tun_fd < 0){
			dnet_log_printf("tunnel stream %04x: error %d\n", t->st.id.id[0] & 0xFFFF, tun_fd);
			t->st.to_exit = 1;
		} else {
			t->st.input_tty = t->st.output_tty = tun_fd;
		}
		if (t->st.tap_to >= 0) {
			uint32_t tap_to = t->st.tap_to;
			data_len = 0;
			do {
				st.data[data_len++] = tap_to;
			} while (tap_to >>= 8);
			goto send;
		}
		}
		break;
	  case DNET_PKT_FORWARDED_TCP:
		if (!memcmp(t->arg, "any:", 4)) {
			struct dnet_ipport *ipport = (struct dnet_ipport *)st.data;
			ipport->ip = t->st.ip_to;
			ipport->port = t->st.port_to;
			dnet_log_printf("stream %04x: tcp forward, to %08x, destination %08x:%04x\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to, ipport->ip, ipport->port);
			data_len = sizeof(struct dnet_ipport);
			goto send;
		} else {
			dnet_log_printf("stream %04x: tcp forward, from %08x, destination %08x:%04x\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to, t->st.ip_to, t->st.port_to);
			t->st.ack += sizeof(struct dnet_ipport);
		}
		break;
	  case DNET_PKT_FORWARDED_UDP:
		st.ack = (uint64_t)t->st.ip_to | (uint64_t)(htons(t->st.port_to)) << 32;
		if (strcmp(t->arg, "any:0")) {
			dnet_log_printf("stream %04x: udp forward, to %08x, destination %08x:%04x\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to, t->st.ip_to, t->st.port_to);
		} else {
			dnet_log_printf("stream %04x: udp forward, from %08x, destination %08x:%04x\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to, t->st.ip_to, t->st.port_to);
		}
		break;
	  case DNET_PKT_FILE_OP:
		dnet_log_printf("stream %04x: copy files, to %08x\n", t->st.id.id[0] & 0xFFFF, t->st.crc_to);
		dnet_file_thread(t, &st);
		t->st.to_exit = 1;
		break;
	}
	while (!t->st.to_exit) {
		if (t->st.pkt_type == DNET_PKT_FORWARDED_UDP) {
			struct sockaddr_in cliaddr;
			socklen_t addrlen = sizeof(cliaddr);
			memset(&cliaddr, 0, sizeof(cliaddr));
			data_len = recvfrom(t->st.input_tty, st.data, DNET_PKT_STREAM_DATA_MAX, 0, (struct sockaddr *)&cliaddr, &addrlen);
			st.seq = (uint64_t)cliaddr.sin_addr.s_addr | (uint64_t)cliaddr.sin_port << 32;
		} else {
			data_len = read(t->st.input_tty, st.data, DNET_PKT_STREAM_DATA_MAX);
		}
		if (t->st.pkt_type == DNET_PKT_SHELL_INPUT) {
			if (g_ctrlc_handled) {
				g_ctrlc_handled = 0;
				data_len = 1;
				st.data[0] = 3;
			}
			if (data_len <= 0) {
				st.data[0] = 4; /* Ctrl-D */
				data_len = 1;
				t->st.to_exit = 1;
			} else {
				int i;
				for (i = 0; i < data_len; ++i) if (st.data[i] == '\n') st.data[i] = '\r';
			}
		} else {
			if (data_len <= 0) {
				if (t->st.pkt_type == DNET_PKT_SHELL_OUTPUT)
					dnet_log_printf("stream %02X: data_len=%d, errno=%d (%s)\n", st.id.id[0] & 0xFFFF, data_len, errno, strerror(errno));
				data_len = 0;
				t->st.to_exit = 1;
			}
		}
	send:
		st.header.length = DNET_PKT_STREAM_MIN_LEN + data_len;
		if (t->st.pkt_type != DNET_PKT_FORWARDED_UDP) {
			st.seq = t->st.seq;
			st.ack = t->st.ack;
		}
		t->st.seq += data_len;
		if (t->st.to_reinit) {
			t->st.to_reinit = 0;
			st.crc_to = t->st.crc_to;
			memcpy(&st.id, &t->st.id, sizeof(struct dnet_stream_id));
		}
//        dnet_log_printf("stream %02X: outcoming packet (%08X,%08X,%08X,%08X)\n", st.id.id[0] & 0xFFFF,
//                ((unsigned *)&st)[0], ((unsigned *)&st)[1], ((unsigned *)&st)[2], ((unsigned *)&st)[3]);
		dnet_send_stream_packet(&st, 0);
	}
	if (t->st.output_tty > 1 && t->st.output_tty != t->st.input_tty) { close(t->st.output_tty); t->st.output_tty = -1; }
#ifndef __LDuS__
	if (t->st.child >= 0){ kill(t->st.child, SIGKILL); t->st.child = -1; }
	if (t->st.input_tty > 0){ close(t->st.input_tty); t->st.input_tty = -1; }
#else
	if (t->st.input_tty > 0 && pipe){ pclose(pipe); t->st.child = -1; t->st.input_tty = -1; }
#endif
	if (t->st.pkt_type == DNET_PKT_SHELL_INPUT)\
		signal(SIGINT, old_handler);
	dnet_log_printf("stream %04x: finished, %d sec, %lld/%lld in/out bytes\n",
		t->st.id.id[0] & 0xFFFF, (int)(time(0) - t->st.creation_time), (long long)t->st.ack, (long long)t->st.seq);
#endif
	t->to_remove = 1;
	return 0;
}

#ifndef CHEATCOIN
static void init_screen(struct termios *oldt) {
	struct termios newt;
	tcgetattr(STDIN_FILENO, oldt);
	newt = *oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	newt.c_cc[VTIME] = 0;
	newt.c_cc[VMIN] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
//    ioctl(STDIN_FILENO, KDSKBMODE, K_MEDIUMRAW);
	printf("\033c");
}

static void finish_screen(struct termios *oldt) {
	printf("\n");
//    ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);
	tcsetattr(STDIN_FILENO, TCSANOW, oldt);
}
#endif

int dnet_stream_main(uint32_t crc_to) {
#ifndef CHEATCOIN
	struct termios oldt;
	struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
	int res;
	if (!t) return 1;
	t->type = DNET_THREAD_STREAM;
	t->st.pkt_type = DNET_PKT_SHELL_INPUT;
	t->st.crc_to = crc_to;
	t->st.to_exit = 0;
	dnet_generate_stream_id(&t->st.id);
	init_screen(&oldt);
	res = dnet_thread_create(t);
	if (res) {
		t->to_remove = 1;
		res = res << 4 | 2;
	} else {
		while (!t->st.to_exit) sleep(1);
	}
	finish_screen(&oldt);
	return res;
#else
	return -1;
#endif
}

static int tunnel_callback(struct dnet_stream *st, void *data) {
	struct dnet_stream *st0 = (struct dnet_stream *)data;
	if (st->pkt_type == DNET_PKT_TUNNELED_MSG && st->tap_from == st0->tap_from) {
		if (st0->tap_to == -1) {
			st->crc_to = st0->crc_to;
			memcpy(&st->id, &st0->id, sizeof(struct dnet_stream_id));
			st->to_reinit = 1;
		}
		return 0x12345678;
	}
	return 0;
}

int dnet_stream_create_tunnel(uint32_t crc_to, int tap_from, int tap_to, struct dnet_stream_id *id) {
	struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
	int res;
	if (!t) return 1;
	t->type = DNET_THREAD_STREAM;
	t->st.pkt_type = DNET_PKT_TUNNELED_MSG;
	t->st.crc_to = crc_to;
	t->st.to_exit = 0;
	t->st.to_reinit = 0;
	t->st.tap_from = tap_from;
	t->st.tap_to = tap_to;
	if (id) memcpy(&t->st.id, id, sizeof(struct dnet_stream_id));
	else dnet_generate_stream_id(&t->st.id);
	res = dnet_traverse_streams(&tunnel_callback, &t->st);
	if (res) {
		if (res == 0x12345678) res = 0;
		else res = res << 4 | 2;
		free(t);
	} else {
		res = dnet_thread_create(t);
		if (res) {
			t->to_remove = 1;
			res = res << 4 | 3;
		}
	}
	return res;
}

int dnet_stream_forward(uint32_t crc_to, int proto, int port_from, uint32_t ip_to, int port_to) {
	struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
	int res;
	if (!t) return 0;
	t->type = DNET_THREAD_FORWARD_FROM;
	t->conn.socket = -1;
	t->st.crc_to = crc_to;
	t->st.ip_to = ip_to;
	t->st.port_to = port_to;
	t->st.proto = proto;
	sprintf(t->argbuf, "any:%u", port_from);
	t->arg = t->argbuf;
	res = dnet_thread_create(t);
	if (res) {
		t->to_remove = 1;
		res = res << 4 | 1;
	}
	return res;
}

struct dnet_traverse_st {
	int (*callback)(struct dnet_stream *st, void *data);
	void *data;
};

static int dnet_traverse_st_callback(struct dnet_thread *t, void *data) {
	if (t->type == DNET_THREAD_STREAM) {
		struct dnet_traverse_st *dts = (struct dnet_traverse_st *)data;
		return (*dts->callback)(&t->st, dts->data);
	} else return 0;
}

int dnet_traverse_streams(int (*callback)(struct dnet_stream *st, void *data), void *data) {
	struct dnet_traverse_st dts;
	dts.callback = callback;
	dts.data = data;
	return dnet_traverse_threads(&dnet_traverse_st_callback, &dts);
}

int dnet_print_stream(struct dnet_stream *st, struct dnet_output *out) {
	struct dnet_host *host = dnet_get_host_by_crc(st->crc_to);
	const char *stream_type;
	int len;
	if (!host) return 0;
	dnet_printf(out, " %2d. %04x ", out->count, st->id.id[0] & 0xFFFF);
	len = dnet_print_host_name(host, out);
	switch (st->pkt_type) {
		case DNET_PKT_SHELL_INPUT:
			stream_type = "shell input";
			break;
		case DNET_PKT_SHELL_OUTPUT:
			stream_type = "shell output";
			break;
		case DNET_PKT_TUNNELED_MSG:
			stream_type = "tunnel";
			break;
		case DNET_PKT_FORWARDED_TCP:
			stream_type = "tcp forward";
			break;
		case DNET_PKT_FORWARDED_UDP:
			stream_type = "udp forward";
			break;
		case DNET_PKT_FILE_OP:
			stream_type = "copy files";
			break;
		default:
			stream_type = "unknown";
			break;
	}
	dnet_printf(out, "%*d sec, %lld/%lld in/out bytes, %s\n",
		28 - (5 + len), (int)(time(0) - st->creation_time), (long long)st->ack, (long long)st->seq, stream_type
		);
	out->count++;
	return 0;
}

struct dnet_find_st {
	struct dnet_packet_stream *p;
	struct dnet_stream *st;
};

static int dnet_find_st_callback(struct dnet_stream *st, void *data) {
	struct dnet_find_st *dfs = (struct dnet_find_st *)data;
	if (!memcmp(&dfs->p->id, &st->id, sizeof(struct dnet_stream_id))) {
		uint64_t stack = (uint64_t)st->ip_to | (uint64_t)(htons(st->port_to)) << 32;
		if (dfs->p->header.type == DNET_PKT_FORWARDED_UDP && dfs->p->seq != stack) return 0;
		dfs->st = st;
		return 1;
	} else return 0;
}

int dnet_process_stream_packet(struct dnet_packet_stream *p) {
	struct dnet_find_st dfs;
	int data_len;
	dfs.p = p;
	if (!dnet_traverse_streams(&dnet_find_st_callback, &dfs)) {
		if (p->header.type == DNET_PKT_SHELL_INPUT && !p->seq && p->header.length == DNET_PKT_STREAM_MIN_LEN + sizeof(struct dnet_tty_params)) {
			struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
			int res;
			if (!t) return 1;
			t->type = DNET_THREAD_STREAM;
			t->st.pkt_type = DNET_PKT_SHELL_OUTPUT;
			t->st.crc_to = p->crc_from;
			t->st.to_exit = 0;
			t->st.to_reinit = 0;
			memcpy(&t->st.id, &p->id, sizeof(struct dnet_stream_id));
			memcpy(&t->st.tty_params, p->data, sizeof(struct dnet_tty_params));
			res = dnet_thread_create(t);
			if (res) {
				t->to_remove = 1;
				return res << 4 | 2;
			}
			return 0;
		} else if (p->header.type == DNET_PKT_FORWARDED_TCP && !p->seq && p->header.length == DNET_PKT_STREAM_MIN_LEN + sizeof(struct dnet_ipport)) {
			struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
			struct dnet_ipport *ipport = (struct dnet_ipport *)p->data;
			int res, ip = ipport->ip, port = ipport->port;
			if (!t) return 3;
			t->type = DNET_THREAD_FORWARD_TO;
			t->conn.socket = -1;
			t->st.pkt_type = DNET_PKT_FORWARDED_TCP;
			t->st.crc_to = p->crc_from;
			t->st.proto = DNET_TCP;
			t->st.ip_to = ip;
			t->st.port_to = port;
			t->st.to_exit = 0;
			t->st.to_reinit = 0;
			t->st.output_tty = -1;
			sprintf(t->argbuf, "%u.%u.%u.%u:%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff, port);
			t->arg = t->argbuf;
			memcpy(&t->st.id, &p->id, sizeof(struct dnet_stream_id));
			res = dnet_thread_create(t);
			if (res) {
				t->to_remove = 1;
				return res << 4 | 4;
			}
			return 0;
		} else if (p->header.type == DNET_PKT_FORWARDED_UDP) {
			struct dnet_thread *t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
			int res, ip = (uint32_t)p->seq, port = ntohs((uint16_t)(p->seq >> 32));
			if (!t) return 3;
			t->type = DNET_THREAD_FORWARD_TO;
			t->conn.socket = -1;
			t->st.pkt_type = DNET_PKT_FORWARDED_UDP;
			t->st.crc_to = p->crc_from;
			t->st.proto = DNET_UDP;
			t->st.ip_to = ip;
			t->st.port_to = port;
			t->st.to_exit = 0;
			t->st.to_reinit = 0;
			t->st.output_tty = -1;
			t->arg = "any:0";
			memcpy(&t->st.id, &p->id, sizeof(struct dnet_stream_id));
			res = dnet_thread_create(t);
			if (res) {
				t->to_remove = 1;
				return res << 4 | 5;
			}
			dfs.st = &t->st;
		} else if (p->header.type == DNET_PKT_TUNNELED_MSG && !p->seq
				&& p->header.length >= DNET_PKT_STREAM_MIN_LEN + 1 && p->header.length <= DNET_PKT_STREAM_MIN_LEN + 4) {
			int len = p->header.length - DNET_PKT_STREAM_MIN_LEN;
			uint32_t tap_to = 0;
			while (len) {
				tap_to <<= 8;
				tap_to += p->data[--len];
			}
			return dnet_stream_create_tunnel(p->crc_from, tap_to, -1, &p->id);
		} else if (p->header.type != DNET_PKT_SHELL_INPUT) return p->header.type << 4 | 5;
		else if (p->seq) return 6;
		else return p->header.length << 4 | 7;
	}
	data_len = p->header.length - DNET_PKT_STREAM_MIN_LEN;
	switch(dfs.st->pkt_type) {
		case DNET_PKT_SHELL_INPUT:
			if (p->header.type != DNET_PKT_SHELL_OUTPUT) return 0x18;
			if (!data_len) { dfs.st->to_exit = 1; return 0; }
			break;
		case DNET_PKT_SHELL_OUTPUT:
			if (p->header.type != DNET_PKT_SHELL_INPUT) return 0x28;
			break;
		case DNET_PKT_FORWARDED_TCP:
		case DNET_PKT_FORWARDED_UDP:
			if (p->header.type != dfs.st->pkt_type) return 0x19;
			if (!data_len) { dfs.st->to_exit = 1; return 0; }
			if (dfs.st->output_tty < 0) {
				int i;
				for (i = 0; i < DNET_FORWARD_WAIT * 100; i++) {
					sched_yield();
					usleep(10000);
					if (dfs.st->to_exit) return 0x29;
					if (dfs.st->output_tty >= 0) break;
				}
				if (dfs.st->output_tty < 0) { dfs.st->to_exit = 1; return 0x39; }
			}
			break;
		default:
			if (p->header.type != dfs.st->pkt_type) return 0x1A;
			break;
	}
	if (p->header.type == DNET_PKT_FORWARDED_UDP) {
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(struct sockaddr_in));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = (uint16_t)(p->ack >> 32);
		servaddr.sin_addr.s_addr = (uint32_t)p->ack;
		if (sendto(dfs.st->output_tty, p->data, data_len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) != data_len) return 0x2A;
	} else {
		if (p->header.type != DNET_PKT_TUNNELED_MSG && p->seq != dfs.st->ack) return 0x3A;
		if (write(dfs.st->output_tty, p->data, data_len) != data_len) return 0x4A;
	}
	dfs.st->ack += data_len;
	return 0;
}

static int dnet_finish_st_callback(struct dnet_stream *st, void *data) {
	uint16_t id = *(uint16_t *)data;
	if ((uint16_t )st->id.id[0] == id) {
		st->to_exit = 1;
		return 1;
	}
	else return 0;
}

int dnet_finish_stream(uint16_t id) {
	return dnet_traverse_streams(&dnet_finish_st_callback, &id);
}
