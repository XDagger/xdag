/* dnet: streams; T11.270-T13.808; $DVS:time$ */

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

void *dnet_thread_stream(void *arg) {
	struct dnet_thread *t = (struct dnet_thread *)arg;
	t->to_remove = 1;
	return 0;
}

int dnet_stream_main(uint32_t crc_to) {
	return -1;
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
