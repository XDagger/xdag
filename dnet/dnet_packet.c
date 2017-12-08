/* dnet: packets; T11.258-T13.714; $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#ifdef __LDuS__
#include <signal.h>
#include <ldus/atomic.h>
#include <ldus/system/kernel.h>
#endif
#include "../dus/programs/dar/source/include/crc.h"
#include "dnet_database.h"
#include "dnet_packet.h"
#include "dnet_connection.h"
#include "dnet_threads.h"
#include "dnet_log.h"
#include "dnet_command.h"
#include "dnet_stream.h"
#include "dnet_files.h"
#include "dnet_main.h"

#define SENT_COMMANDS_MAX		256
#define CHEATCOIN_PACKET_LEN	512

struct dnet_sent_command {
	struct dnet_stream_id id;
	struct dnet_output out;
	char valid;
};

struct dnet_stream_id g_last_received_command_id  =  { { 0, 0, 0, 0} };
struct dnet_sent_command *g_last_sent_command = 0;
static int (*cheatcoin_callback)(void *packet, void *connection) = 0;

int dnet_set_cheatcoin_callback(int (*callback)(void *, void *)) { cheatcoin_callback = callback; return 0; }

int dnet_send_packet(struct dnet_packet *p, struct dnet_connection *conn) {
    char buf[512];
    int i, len = (512 - p->header.length) & 511;
    p->header.crc32 = 0;
    p->header.crc32 = crc_of_array((uint8_t *)p, p->header.length);
    for (i = 0; i < len; ++i) buf[i] = rand() % DNET_PKT_MIN;
	dthread_mutex_lock(&conn->mutex);
    if (conn->sess) {
		ssize_t res = dnet_session_write(conn->sess, p, p->header.length);
		if (len) dnet_session_write(conn->sess, buf, len);
		if (res == p->header.length) conn->counters[DNET_C_OUT_PACKETS]++;
		else conn->counters[DNET_C_OUT_DROPPED]++;
	}
	dthread_mutex_unlock(&conn->mutex);
    return 0;
}

struct cheatcoin_data {
	struct dnet_packet *p;
	struct dnet_connection *conn;
	int nconn;
};

static int dnet_send_cheatcoin_callback(struct dnet_connection *conn, void *data) {
	struct cheatcoin_data *d = (struct cheatcoin_data *)data;
	if (d->nconn < 0) {
		if (conn != d->conn) dnet_send_packet(d->p, conn);
	} else {
		if (!(rand() % ++d->nconn)) d->conn = conn;
	}
	return 0;
}

void *dnet_send_cheatcoin_packet(void *block, void *connection_to) {
	struct cheatcoin_data d;
	d.conn = (struct dnet_connection *)connection_to;
	d.p = (struct dnet_packet *)block;
	d.p->header.type = DNET_PKT_CHEATCOIN;
	d.p->header.length = CHEATCOIN_PACKET_LEN;
	if (!d.conn) {
		d.p->header.ttl = 1;
		d.nconn = 0;
	} else if ((unsigned long)d.conn < 256) {
		d.p->header.ttl = (uint8_t)(unsigned long)d.conn;
		d.conn = 0;
		d.nconn = -1;
	} else if ((long)d.conn & 1) {
		if (d.p->header.ttl <= 2) return 0;
		d.p->header.ttl--;
		d.conn = (struct dnet_connection *)((long)d.conn - 1);
		d.nconn = -1;
	} else {
		d.p->header.ttl = 1;
		d.nconn = INT_MAX;
	}
	if (d.nconn <= 0) dnet_traverse_connections(dnet_send_cheatcoin_callback, &d);
	if (d.nconn > 0) dnet_send_packet(d.p, d.conn);
	return (d.nconn > 0 && d.nconn < INT_MAX ? d.conn : 0);
}

int dnet_send_command_packet(struct dnet_packet_stream *st, struct dnet_output *output) {
    int i;
	if (!g_last_sent_command) {
		g_last_sent_command = calloc(SENT_COMMANDS_MAX, sizeof(struct dnet_sent_command));
		if (!g_last_sent_command) return 4;
    }
	for (i = 0; i < SENT_COMMANDS_MAX; ++i) if (!g_last_sent_command[i].valid ||
			(output->f ? (g_last_sent_command[i].out.f == output->f) : (g_last_sent_command[i].out.str == output->str))) {
		g_last_sent_command[i].valid = 1;
		memcpy(&g_last_sent_command[i].out, output, sizeof(struct dnet_output));
		memcpy(&g_last_sent_command[i].id, &st->id, sizeof(struct dnet_stream_id));
		return dnet_send_stream_packet(st, 0);
    }
    return 5;
}

int dnet_cancel_command(struct dnet_output *output) {
    int i;
	if (g_last_sent_command) for (i = 0; i < SENT_COMMANDS_MAX; ++i) if (g_last_sent_command[i].valid
			&& (output->f ? (g_last_sent_command[i].out.f == output->f) : (g_last_sent_command[i].out.str == output->str))) {
		g_last_sent_command[i].valid = 0;
		return 0;
    }
    return 1;
}

#ifndef CHEATCOIN
static void dnet_pkt_command_callback(struct dnet_output *out) {
    struct dnet_packet *p = (struct dnet_packet *)out->data;
    p->header.length = DNET_PKT_STREAM_MAX_LEN - out->len;
	out->err |= dnet_send_stream_packet(&p->st, 0);
	out->str = (char *)p->st.data;
    out->len = DNET_PKT_STREAM_DATA_MAX;
}

static void *dnet_process_command_thread(void *arg) {
	struct dnet_output out;
	struct dnet_packet *p = (struct dnet_packet *)arg;
	out.f = 0;
	out.str = (char *)p->st.data;
	out.len = DNET_PKT_STREAM_DATA_MAX;
	out.callback = &dnet_pkt_command_callback;
	out.data = p;
	out.err = 0;
	p->header.type = DNET_PKT_COMMAND_OUTPUT;
	p->header.ttl = 16;
	p->st.data[p->header.length - DNET_PKT_STREAM_MIN_LEN] = 0;
	p->st.ack = p->st.seq + p->header.length - DNET_PKT_STREAM_MIN_LEN;
	p->st.seq = 0;
	{ uint32_t tmp = p->st.crc_from; p->st.crc_from = p->st.crc_to; p->st.crc_to = tmp; }
	dnet_command((char *)p->st.data, &out);
	free(p);
	return 0;
}
#endif

int dnet_process_packet(struct dnet_packet *p, struct dnet_connection *conn) {
    uint32_t crc = p->header.crc32;
#ifndef CHEATCOIN
	int res;
#endif
	p->header.crc32 = 0;
    if (crc_of_array((uint8_t *)p, p->header.length) != crc) return 1;
    switch(p->header.type) {
        case DNET_PKT_EXCHANGE:
            if (p->header.length < DNET_PKT_EXCHANGE_MIN_LEN || p->header.length > DNET_PKT_EXCHANGE_MAX_LEN) return 0x12;
            {
				struct dnet_host *host = dnet_add_host(&p->ex.pub_key, p->ex.time_ago, conn->ipaddr, conn->port, DNET_ROUTE_AUTO);
                if (!host) return 3;
				if (p->header.length > DNET_PKT_EXCHANGE_MIN_LEN) {
					struct dnet_host *host_from = dnet_session_get_host(conn->sess);
					int namelen = p->header.length - DNET_PKT_EXCHANGE_MIN_LEN;
					if (namelen && (!host->name_len || (host_from && host_from->is_trusted) || !host->is_trusted)) {
						char *version = memchr(p->ex.name, 0, namelen);
						dnet_set_host_name(host, p->ex.name, version ? version - p->ex.name : namelen);
						if (version) {
							namelen -= version - p->ex.name + 1;
							if (namelen) {
								memmove(version, version + 1, namelen);
								version[namelen] = 0;
								dnet_set_host_version(host, version);
							}
						}
					}
				}
            }
            break;
        case DNET_PKT_UPDATE:
            if (p->header.length < DNET_PKT_UPDATE_MIN_LEN || p->header.length > DNET_PKT_UPDATE_MAX_LEN ||
                    (p->header.length - DNET_PKT_UPDATE_MIN_LEN) % sizeof(struct dnet_packet_update_item)) return 0x22;
            {
                struct dnet_host *host;
                int i, nitems = (p->header.length - DNET_PKT_UPDATE_MIN_LEN) / sizeof(struct dnet_packet_update_item);
                for (i = 0; i < nitems; ++i) {
                    host = dnet_get_host_by_crc(p->up.item[i].crc32);
					if (host) dnet_update_host(host, p->up.item[i].time_ago, conn->ipaddr, conn->port, DNET_ROUTE_AUTO);
                }
            }
	    break;
	case DNET_PKT_COMMAND:
	case DNET_PKT_COMMAND_OUTPUT:
	case DNET_PKT_SHELL_INPUT:
	case DNET_PKT_SHELL_OUTPUT:
	case DNET_PKT_TUNNELED_MSG:
	case DNET_PKT_FORWARDED_TCP:
	case DNET_PKT_FORWARDED_UDP:
	case DNET_PKT_FILE_OP:
#ifndef CHEATCOIN
		if (p->header.length < DNET_PKT_STREAM_MIN_LEN || p->header.length > DNET_PKT_STREAM_MAX_LEN) return 0x13;
	    {
		struct dnet_host *host = dnet_get_self_host();
		if (host->crc32 == p->st.crc_to) {
			struct dnet_host *host_from = dnet_get_host_by_crc(p->st.crc_from);
			if (!host_from) return 0x23;
			if (!host_from->is_trusted) return 0x33;
		    switch(p->header.type) {
			case DNET_PKT_COMMAND:
				if (!memcmp(&g_last_received_command_id, &p->st.id, sizeof(struct dnet_stream_id))) return 0x43;
			    memcpy(&g_last_received_command_id, &p->st.id, sizeof(struct dnet_stream_id));
				{
					struct dnet_packet *p1 = malloc(sizeof(struct dnet_packet) + 1);
					pthread_t t;
					if (!p1) return 0x53;
					memcpy(p1, p, p->header.length);
					if (pthread_create(&t, 0, &dnet_process_command_thread, p1)) { free(p1); return 0x63; }
#ifndef __LDuS__
					pthread_detach(t);
#endif
				}
			    break;
			case DNET_PKT_COMMAND_OUTPUT:
				{
				struct dnet_output *o = 0;
				if (g_last_sent_command) {
					int i;
					for (i = 0; i < SENT_COMMANDS_MAX; ++i) {
						if (g_last_sent_command[i].valid && !memcmp(&g_last_sent_command[i].id, &p->st.id, sizeof(struct dnet_stream_id))) {
							o = &g_last_sent_command[i].out;
							break;
						}
					}
			    }
				if (o) {
					int len = p->header.length - DNET_PKT_STREAM_MIN_LEN;
					if (len) dnet_write(o, p->st.data, len);
					else {
#ifdef __LDuS__
						if (o->f) {
							pid_t pid;
							if ((pid = ldus_atomic_cmpxchg(&o->f->pid, o->f->pid, 0)))
							ldus_kill_task(pid, SIGCONT);
						}
#endif
						dnet_cancel_command(o);
					}
				} else return 0x73;
				}
			    break;
			case DNET_PKT_SHELL_INPUT:
			case DNET_PKT_SHELL_OUTPUT:
			case DNET_PKT_TUNNELED_MSG:
			case DNET_PKT_FORWARDED_TCP:
			case DNET_PKT_FORWARDED_UDP:
				res = dnet_process_stream_packet(&p->st);
			    if (res) res = res << 4 | 4;
			    return res;
			case DNET_PKT_FILE_OP:
				res = dnet_process_file_packet(&p->st);
				if (res) res = res << 4 | 5;
				return res;
			default:
				return 6;
		    }
		} else if (p->header.ttl >= 2) {
		    p->header.ttl--;
			if (dnet_send_stream_packet(&p->st, conn)) return 7;
		} else return 8;
	    }
#endif
	    break;
	case DNET_PKT_CHEATCOIN:
		{
			uint8_t ttl = p->header.ttl;
			int res;
			if (p->header.length != CHEATCOIN_PACKET_LEN) return 0x19;
			if (!cheatcoin_callback) return 0x29;
			res = (*cheatcoin_callback)(p, conn);
			if (res < 0) return 0x39;
			if (res > 0 && ttl > 2) {
				p->header.ttl = ttl;
				dnet_send_cheatcoin_packet(p, (void *)((long)conn | 1));
			}
		}
		break;
	default:
			return 9;
    }
    return 0;
}
