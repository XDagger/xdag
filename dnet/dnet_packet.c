/* dnet: packets; T11.258-T13.808; $DVS:time$ */

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
#define XDAG_PACKET_LEN	512

struct dnet_sent_command {
	struct dnet_stream_id id;
	struct dnet_output out;
	char valid;
};

struct dnet_stream_id g_last_received_command_id  =  { { 0, 0, 0, 0} };
struct dnet_sent_command *g_last_sent_command = 0;
static int (*xdag_callback)(void *packet, void *connection) = 0;

int dnet_set_xdag_callback(int (*callback)(void *, void *)) { xdag_callback = callback; return 0; }

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

struct xdag_data {
	struct dnet_packet *p;
	struct dnet_connection *conn;
	int nconn;
};

static int dnet_send_xdag_callback(struct dnet_connection *conn, void *data) {
	struct xdag_data *d = (struct xdag_data *)data;
	if (d->nconn < 0) {
		if (conn != d->conn) dnet_send_packet(d->p, conn);
	} else {
		if (!(rand() % ++d->nconn)) d->conn = conn;
	}
	return 0;
}

void *dnet_send_xdag_packet(void *block, void *connection_to) {
	struct xdag_data d;
	d.conn = (struct dnet_connection *)connection_to;
	d.p = (struct dnet_packet *)block;
	d.p->header.type = DNET_PKT_XDAG;
	d.p->header.length = XDAG_PACKET_LEN;
	if (!d.conn) {
		d.p->header.ttl = 1;
		d.nconn = 0;
	} else if ((uintptr_t)d.conn < 256) {
		d.p->header.ttl = (uint8_t)(uintptr_t)d.conn;
		d.conn = 0;
		d.nconn = -1;
	} else if ((uintptr_t)d.conn & 1) {
		if (d.p->header.ttl <= 2) return 0;
		d.p->header.ttl--;
		d.conn = (struct dnet_connection *)((uintptr_t)d.conn - 1);
		d.nconn = -1;
	} else {
		d.p->header.ttl = 1;
		d.nconn = INT_MAX;
	}
	if (d.nconn <= 0) dnet_traverse_connections(dnet_send_xdag_callback, &d);
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

int dnet_process_packet(struct dnet_packet *p, struct dnet_connection *conn) {
    uint32_t crc = p->header.crc32;
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
	    break;
	case DNET_PKT_XDAG:
		{
			uint8_t ttl = p->header.ttl;
			int res;
			if (p->header.length != XDAG_PACKET_LEN) return 0x19;
			if (!xdag_callback) return 0x29;
			res = (*xdag_callback)(p, conn);
			if (res < 0) return 0x39;
			if (res > 0 && ttl > 2) {
				p->header.ttl = ttl;
				dnet_send_xdag_packet(p, (void *)((uintptr_t)conn | 1));
			}
		}
		break;
	default:
			return 9;
    }
    return 0;
}
