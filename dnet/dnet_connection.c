/* dnet: connections; T11.253-T13.444; $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "dnet_connection.h"
#include "dnet_packet.h"
#include "dnet_threads.h"

static ssize_t dnet_conn_read(void *private_data, void *buf, size_t size) {
    struct dnet_connection *conn = (struct dnet_connection *)private_data;
    struct dnet_packet *p = (struct dnet_packet *)conn->buf;
    ssize_t res = 0;
    size_t todo, packet_size;
    while (size) {
        if (!conn->buf_pos) packet_size = 1;
        else if (conn->buf_pos < sizeof(struct dnet_packet_header)) packet_size = sizeof(struct dnet_packet_header);
        else packet_size = p->header.length;
        todo = packet_size - conn->buf_pos;
        if (todo > size) todo = size;
        memcpy(conn->buf + conn->buf_pos, buf, todo);
        conn->buf_pos += todo;
		buf = (uint8_t *)buf + todo;
        size -= todo;
        res += todo;
        if (conn->buf_pos == packet_size) {
            if (packet_size == 1) {
                if (p->header.type < DNET_PKT_MIN || p->header.type > DNET_PKT_MAX) conn->buf_pos = 0;
            } else if (packet_size == sizeof(struct dnet_packet_header)) {
				if (p->header.length <= sizeof(struct dnet_packet_header) || p->header.length > sizeof(conn->buf)) conn->buf_pos = 0;
            } else {
                int err;
                conn->counters[DNET_C_IN_PACKETS]++;
                if ((err = dnet_process_packet(p, conn))) {
                    conn->counters[DNET_C_IN_DROPPED]++;
                    dnet_log_printf("incoming packet (%08X,%08X,%08X,%08X...) dropped, reason = %X\n",
                                    ((uint32_t *)p)[0], ((uint32_t *)p)[1], ((uint32_t *)p)[2], ((uint32_t *)p)[3], err);
                }
                conn->buf_pos = 0;
            }
        }
    }
    return res;
}

static ssize_t dnet_conn_write(void *private_data, void *buf, size_t size) {
    struct dnet_connection *conn = (struct dnet_connection *)private_data;
    ssize_t res = 0, done;
    while (size) {
        done = write(conn->socket, buf, size);
        if (done < 0) break;
        res += done;
        size -= done;
		buf = (uint8_t *)buf + done;
        conn->counters[DNET_C_OUT_BYTES] += done;
    }
    return res;
}

static const struct dnet_session_ops dnet_conn_ops = {
    &dnet_conn_read,
    &dnet_conn_write,
};

static inline ssize_t dnet_socket_read(int fd, void *buf, size_t size) {
#ifdef __LDuS__
    return read(fd, buf, size);
#else
    struct timeval timeout = { DNET_UPDATE_PERIOD * 3 / 2, 0 };
    while (timeout.tv_sec || timeout.tv_usec) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(fd, &rset);
        if (select(fd + 1, &rset, 0, 0, &timeout) < 0) {
            dnet_log_printf("dnet: select returns an error, %s", strerror(errno));
        }
        if (FD_ISSET(fd, &rset)) {
            return read(fd, buf, size);
        }
    }
    return -1;
#endif
}

int dnet_connection_main(struct dnet_connection *conn) {
	struct dnet_thread *thread = 0;
    char buf[0x1000];
    ssize_t size;
    int res = 0;
    conn->buf_pos = 0;
    conn->is_new = 0;
    memset(conn->counters, 0, DNET_C_END * sizeof(uint64_t));
	conn->sess = dnet_session_create(conn, &dnet_conn_ops, conn->ipaddr, conn->port);
    if (!conn->sess) { res = 1; goto end; }
    conn->creation_time = conn->last_active_time = time(0);
    res = dnet_session_init(conn->sess);
    if (res) { res = res * 10 + 2; goto end; }
    while ((size = dnet_socket_read(conn->socket, buf, 0x1000)) > 0) {
        conn->last_active_time = time(0);
        conn->counters[DNET_C_IN_BYTES] += size;
        dnet_session_read(conn->sess, buf, size);
        if (conn->counters[DNET_C_IN_BYTES] >= sizeof(struct dnet_key) + 512
				&& conn->counters[DNET_C_IN_BYTES] - size < sizeof(struct dnet_key) + 512) {
            conn->is_new = 1;
			thread = malloc(sizeof(struct dnet_thread));
			if (!thread) { res = 3; goto end; }
			thread->arg = (const char *)conn;
			thread->conn.socket = -1;
			thread->type = DNET_THREAD_EXCHANGER;
			res = dnet_thread_create(thread);
			if (res) { thread->to_remove = 1; res = 4; goto end; }
		}
    }
	res = (int)size * 10 + 4;
end:
	if (thread) {
		thread->arg = 0;
	}
    if (conn->sess) {
		dthread_mutex_lock(&conn->mutex);
		if (conn->sess) {
			struct dnet_host *host = dnet_session_get_host(conn->sess);
			if (host && host->route_type == DNET_ROUTE_IMMEDIATE)
				host->route_type = DNET_ROUTE_AUTO;
			free(conn->sess);
			conn->sess = 0;
		}
		dthread_mutex_unlock(&conn->mutex);
    }
    return res;
}

struct dnet_traverse_conn {
    int (*callback)(struct dnet_connection *conn, void *data);
    void *data;
};

static int dnet_traverse_conn_callback(struct dnet_thread *t, void *data) {
    if ((t->type == DNET_THREAD_CLIENT || t->type == DNET_THREAD_ACCEPTED) && t->conn.sess) {
        struct dnet_traverse_conn *dtc = (struct dnet_traverse_conn *)data;
        return (*dtc->callback)(&t->conn, dtc->data);
    } else return 0;
}

int dnet_traverse_connections(int (*callback)(struct dnet_connection *conn, void *data), void *data) {
    struct dnet_traverse_conn dtc;
    dtc.callback = callback;
    dtc.data = data;
    return dnet_traverse_threads(&dnet_traverse_conn_callback, &dtc);
}

int dnet_print_connection(struct dnet_connection *conn, struct dnet_output *out) {
    int len;
    dnet_printf(out, " %2d. ", out->count);
    dnet_printf(out, "%d.%d.%d.%d:%d%n", conn->ipaddr >> 24 & 0xFF, conn->ipaddr >> 16 & 0xFF, conn->ipaddr >> 8 & 0xFF, conn->ipaddr & 0xFF,
       conn->port, &len);
    dnet_printf(out, "%*d sec, %lld/%lld in/out bytes, %lld/%lld packets, %lld/%lld dropped\n",
		28 - len, (int)(time(0) - conn->creation_time),
		(long long)conn->counters[DNET_C_IN_BYTES], (long long)conn->counters[DNET_C_OUT_BYTES],
		(long long)conn->counters[DNET_C_IN_PACKETS], (long long)conn->counters[DNET_C_OUT_PACKETS],
		(long long)conn->counters[DNET_C_IN_DROPPED], (long long)conn->counters[DNET_C_OUT_DROPPED]
    );
    out->count++;
    return 0;
}
