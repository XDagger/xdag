/* dnet: threads; T11.231-T13.808; $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __LDuS__
#include <ldus/system/network.h>
#endif
#include "system.h"
#include "dnet_threads.h"
#include "dnet_connection.h"
#include "dnet_packet.h"
#include "dnet_database.h"
#include "dnet_log.h"
#include "dnet_stream.h"

#define CONNECTIONS_MAX	    100
#define MAX_N_INBOUND		CONNECTIONS_MAX
#define EXCHANGE_PERIOD	    1800
#define EXCHANGE_MAX_TIME	(3600 * 48)
#define LOG_PERIOD          300
#define GC_PERIOD           60
#define UPDATE_PERIOD	    DNET_UPDATE_PERIOD

struct list *g_threads;
pthread_rwlock_t g_threads_rwlock;
int g_nthread;
int(*dnet_connection_open_check)(void *conn, uint32_t ip, uint16_t port);
void(*dnet_connection_close_notify)(void *conn) = 0;
static int g_n_inbound = 0;
int g_conn_limit = MAX_N_INBOUND;

static void dnet_thread_work(struct dnet_thread *t)
{
	char buf[0x100];
	const char *mess, *mess1 = "";
	struct sockaddr_in peeraddr;
	struct hostent *host;
	char *str, *lasts;
	int res = 0, ip_to = 0, port_to = 0, crc_to = 0, proto = DNET_TCP, reuseaddr = 1;

	if (t->type == DNET_THREAD_FORWARD_FROM || t->type == DNET_THREAD_STREAM) {
		port_to = t->st.port_to;
		ip_to = t->st.ip_to;
		crc_to = t->st.crc_to;
		proto = t->st.proto;
	}

	// Create a socket
	t->conn.socket = socket(AF_INET, (proto == DNET_TCP ? SOCK_STREAM : SOCK_DGRAM), IPPROTO_TCP);
	if (t->conn.socket == INVALID_SOCKET) {
		mess = "cannot create a socket";
		goto err;
	}
	if (fcntl(t->conn.socket, F_SETFD, FD_CLOEXEC) == -1) {
		dnet_log_printf("dnet.%d: can't set FD_CLOEXEC flag on socket, %s\n", t->nthread, strerror(errno));
	}

	// Fill in the address of server
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, t->arg);
	str = strtok_r(buf, " \t\r\n:", &lasts);
	if (!str) { mess = "host is not given"; goto err; }
	if (!strcmp(str, "any")) {
		peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if (!inet_aton(str, &peeraddr.sin_addr)) {
		host = gethostbyname(str);
		if (!host || !host->h_addr_list[0]) {
			mess = "cannot resolve host ", mess1 = str; res = h_errno;
			goto err;
		}
		// Write resolved IP address of a server to the address structure
		memmove(&peeraddr.sin_addr.s_addr, host->h_addr_list[0], 4);
	}
	t->conn.ipaddr = ntohl(peeraddr.sin_addr.s_addr);

	// Resolve port
	str = strtok_r(0, " \t\r\n:", &lasts);
	if (!str) { mess = "port is not given"; goto err; }
	t->conn.port = atoi(str);
	peeraddr.sin_port = htons(t->conn.port);

	if (t->type == DNET_THREAD_SERVER || t->type == DNET_THREAD_FORWARD_FROM || proto == DNET_UDP) {
		struct linger linger_opt = { 1, (proto == DNET_UDP ? 5 : 0) }; // Linger active, timeout 5
		socklen_t peeraddr_len = sizeof(peeraddr);

		// Bind a socket to the address
#ifdef __LDuS__
		if (t->type == DNET_THREAD_SERVER) {
			int i;
			for (i = 0; i < 2; ++i)
				ldus_free_network_port(AF_INET, SOCK_STREAM, 0, i, t->conn.port);
		}
#endif
		res = bind(t->conn.socket, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
		if (res) {
			mess = "cannot bind a socket";
			goto err;
		}

		// Set the "LINGER" timeout to zero, to close the listen socket
		// immediately at program termination.
		setsockopt(t->conn.socket, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
		setsockopt(t->conn.socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));

		if (proto == DNET_UDP) {
			if (t->type != DNET_THREAD_STREAM) {
				dnet_generate_stream_id(&t->st.id);
				t->type = DNET_THREAD_STREAM;
				t->st.pkt_type = DNET_PKT_FORWARDED_UDP;
			}
			t->st.crc_to = crc_to;
			t->st.ip_to = ip_to;
			t->st.port_to = port_to;
			t->st.input_tty = t->st.output_tty = t->conn.socket;
			dnet_thread_stream(t);
			return;
		}

		// Now, listen for a connection
		res = listen(t->conn.socket, CONNECTIONS_MAX);    // "1" is the maximal length of the queue
		if (res) {
			mess = "cannot listen";
			goto err;
		}

		for (;;) {
			struct dnet_thread *t1 = malloc(sizeof(struct dnet_thread));
			if (!t1) { mess = "allocation error"; goto err; }
			*t1 = *t;
			// Accept a connection (the "accept" command waits for a connection with
			// no timeout limit...)
begin:
			t1->conn.socket = accept(t->conn.socket, (struct sockaddr*) &peeraddr, &peeraddr_len);
			if (t1->conn.socket < 0) { free(t1); mess = "cannot accept"; goto err; }
			setsockopt(t1->conn.socket, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
			setsockopt(t1->conn.socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));
			if (fcntl(t1->conn.socket, F_SETFD, FD_CLOEXEC) == -1) {
				dnet_log_printf("dnet.%d: can't set FD_CLOEXEC flag on accepted socket, %s\n", t->nthread, strerror(errno));
			}
			if (g_n_inbound >= g_conn_limit || (dnet_connection_open_check
				&& (*dnet_connection_open_check)(&t1->conn, peeraddr.sin_addr.s_addr, ntohs(peeraddr.sin_port)))) {
				close(t1->conn.socket); goto begin;
			}
			if (t->type == DNET_THREAD_FORWARD_FROM) {
				t1->type = DNET_THREAD_STREAM;
				t1->st.pkt_type = DNET_PKT_FORWARDED_TCP;
				t1->st.to_exit = 0;
				t1->st.to_reinit = 0;
				t1->st.input_tty = t1->st.output_tty = t1->conn.socket;
				t1->st.crc_to = crc_to;
				t1->st.ip_to = ip_to;
				t1->st.port_to = port_to;
				dnet_generate_stream_id(&t1->st.id);
			} else {
				t1->conn.ipaddr = ntohl(peeraddr.sin_addr.s_addr);
				t1->conn.port = ntohs(peeraddr.sin_port);
				t1->type = DNET_THREAD_ACCEPTED;
			}
			res = dnet_thread_create(t1);
			if (res) {
				if (t1->conn.socket >= 0) {
					close(t1->conn.socket); t1->conn.socket = -1;
				}
				//pthread_mutex_lock(&t->conn.mutex);
				t1->to_remove = 1;
				//pthread_mutex_unlock(&t->conn.mutex);
				mess = "can't create new thread"; goto err;
			} else {
				g_n_inbound++;
			}
		}
	} else {
		// Connect to a remote server
		res = connect(t->conn.socket, (struct sockaddr*) &peeraddr, sizeof(peeraddr));
		if (res) {
			mess = "cannot connect";
			goto err;
		}

		if (t->type == DNET_THREAD_STREAM) {
			t->st.ip_to = ip_to;
			t->st.port_to = port_to;
			t->st.input_tty = t->st.output_tty = t->conn.socket;
			dnet_thread_stream(t);
		} else {
			res = dnet_connection_main(&t->conn);
			if (res) {
				mess = "connection error"; 
				goto err;
			}
		}
	}

	return;
err:
#ifdef QDNET
	if (strcmp(mess, "cannot connect") && strcmp(mess, "connection error"))
#endif
		dnet_log_printf("dnet.%d: %s%s (%d), %s\n", t->nthread, mess, mess1, res, strerror(errno));
}

static void *dnet_thread_client_server(void *arg)
{
	struct dnet_thread *t = (struct dnet_thread *)arg;
	extern int g_xdag_sync_on;
	while (!g_xdag_sync_on) sleep(1);
	for (;;) {
#ifndef QDNET
		if (dnet_limited_version) dnet_log_printf("dnet.%d: starting connection.\n", t->nthread);
		else dnet_log_printf("dnet.%d: starting connection with %s.\n", t->nthread, t->arg);
#endif
		dnet_thread_work(t);
		if (t->conn.socket >= 0) {
			close(t->conn.socket); t->conn.socket = -1;
		}
		if (dnet_connection_close_notify) (*dnet_connection_close_notify)(&t->conn);
		t->to_remove = 1;
		break;
	}
	return 0;
}

static void *dnet_thread_accepted(void *arg)
{
	struct dnet_thread *t = (struct dnet_thread *)arg;
	int ip = t->conn.ipaddr, port = t->conn.port;
	dnet_log_printf("dnet.%d: received connection from %u.%u.%u.%u:%u\n", t->nthread, ip >> 24 & 0xff, ip >> 16 & 0xff, ip >> 8 & 0xff, ip & 0xff, port);
	int res = dnet_connection_main(&t->conn);
	if (res) {
		const char *mess = "connection error";
		dnet_log_printf("dnet.%d: %s (%d), %s\n", t->nthread, mess, res, strerror(errno));
	}
	close(t->conn.socket);
	g_n_inbound--;
	t->conn.socket = -1;
	//pthread_mutex_lock(&t->conn.mutex);
	if (dnet_connection_close_notify) (*dnet_connection_close_notify)(&t->conn);
	t->to_remove = 1;
	//pthread_mutex_unlock(&t->conn.mutex);
	return 0;
}

int dnet_traverse_threads(int(*callback)(struct dnet_thread *, void *), void *data)
{
	int res = 0;
	struct list *l;
	pthread_rwlock_rdlock(&g_threads_rwlock);
	for (l = g_threads->next; l != g_threads; l = l->next) {
		struct dnet_thread *t = container_of(l, struct dnet_thread, threads);
		if (!t->to_remove) {
			res = (*callback)(t, data);
			if (res) break;
		}
	}
	pthread_rwlock_unlock(&g_threads_rwlock);
	return res;
}

static int dnet_garbage_collect(void)
{
	struct list *l, *lnext;
	int total = 0, collected = 0;
	dnet_log_printf("dnet gc: start to collect\n");
	pthread_rwlock_wrlock(&g_threads_rwlock);
	for (l = g_threads->next; l != g_threads; l = lnext) {
		struct dnet_thread *t = container_of(l, struct dnet_thread, threads);
		lnext = l->next;
		total++;
		if (t->to_remove == 2) {
			list_remove(l);
			dthread_mutex_destroy(&t->conn.mutex);
			free(t);
			collected++;
		} else if (t->to_remove) t->to_remove = 2;
	}
	pthread_rwlock_unlock(&g_threads_rwlock);
	dnet_log_printf("dnet gc: %d threads total, %d collected\n", total, collected);
	return 0;
}

static void *dnet_thread_collector(void __attribute__((unused)) *arg)
{
	time_t t = 0, gc_t = 0;
	for (;;) {
		t = time(0);
		if (t - gc_t >= GC_PERIOD) {
			gc_t = t;
			dnet_garbage_collect();
		}
		sleep(GC_PERIOD / 10);
	}
	return 0;

}

static void dnet_fill_exchange_packet(struct dnet_host *host, struct dnet_packet_exchange *ex, time_t now)
{
	ex->header.type = DNET_PKT_EXCHANGE;
	ex->header.ttl = 1;
	ex->header.length = DNET_PKT_EXCHANGE_MIN_LEN;
	memcpy(ex->pub_key.key, host->key.key, sizeof(struct dnet_key));
	ex->time_ago = dnet_host_time_ago(host, now);
	if (host->name_len) {
		memcpy(ex->name, host->name, host->name_len);
		ex->header.length += host->name_len;
	}
	if (strlen(host->version) && host->name_len + strlen(host->version) + 1 <= DNET_HOST_NAME_MAX) {
		ex->name[host->name_len] = 0;
		memcpy(ex->name + host->name_len + 1, host->version, strlen(host->version));
		ex->header.length += strlen(host->version) + 1;
	}
}

struct host_exchange_data {
	struct dnet_connection *conn;
	time_t now;
};

static int dnet_host_exchange_callback(struct dnet_host *host, void *data)
{
	struct host_exchange_data *ed = (struct host_exchange_data *)data;
	if (host->last_appear >= ed->conn->last_host_exchange_time && dnet_host_time_ago(host, ed->now) <= EXCHANGE_MAX_TIME) {
		struct dnet_packet_exchange ex;
		dnet_fill_exchange_packet(host, &ex, ed->now);
		dnet_send_packet((struct dnet_packet *)&ex, ed->conn);
	}
	return 0;
}

struct host_update_data {
	struct dnet_packet_update up;
	struct dnet_connection *conn;
	time_t now;
};

static int dnet_host_update_callback(struct dnet_host *host, void *data)
{
	struct host_update_data *ud = (struct host_update_data *)data;
	int nitem;
	if (!host->is_local && host->last_time_changed + DNET_ACTIVE_PERIOD < ud->now) return 0;
	if (ud->up.header.length >= DNET_PKT_UPDATE_MAX_LEN) {
		dnet_send_packet((struct dnet_packet *)&ud->up, ud->conn);
		ud->up.header.length = DNET_PKT_UPDATE_MIN_LEN;
	}
	nitem = (ud->up.header.length - DNET_PKT_UPDATE_MIN_LEN) / sizeof(struct dnet_packet_update_item);
	ud->up.item[nitem].crc32 = host->crc32;
	ud->up.item[nitem].time_ago = dnet_host_time_ago(host, ud->now);
	ud->up.header.length += sizeof(struct dnet_packet_update_item);
	return 0;
}

static void *dnet_thread_exchanger(void *arg)
{
	struct dnet_thread *t = (struct dnet_thread *)arg;
	struct dnet_connection *conn;
	time_t ex_t = 0, up_t = 0;
	while ((conn = (struct dnet_connection *)t->arg)) {
		time_t now = time(0);
		if (conn->is_new || now - ex_t >= EXCHANGE_PERIOD) {
			ex_t = now;
			conn->last_host_exchange_time = (conn->is_new ? 0 : now - 2 * EXCHANGE_PERIOD);
			conn->is_new = 0;
		}
		if (now > conn->last_host_exchange_time) {
			struct host_exchange_data ed;
			ed.conn = conn;
			ed.now = now;
			dnet_traverse_hosts(&dnet_host_exchange_callback, &ed);
			conn->last_host_exchange_time = now;
		}
		if (now - up_t >= UPDATE_PERIOD) {
			struct host_update_data ud;
			up_t = now;
			ud.now = now;
			ud.conn = conn;
			ud.up.header.type = DNET_PKT_UPDATE;
			ud.up.header.ttl = 1;
			ud.up.header.length = DNET_PKT_UPDATE_MIN_LEN;
			dnet_traverse_hosts(&dnet_host_update_callback, &ud);
			dnet_send_packet((struct dnet_packet *)&ud.up, conn);
		}
		sleep(1);
	}
	t->to_remove = 1;
	return 0;
}

static int dnet_conn_count_callback(struct dnet_connection *conn, void *data)
{
	if (data && (time(0) - conn->last_active_time) < UPDATE_PERIOD * 3 / 2)++*(int *)data;
	return 0;
}

static void *dnet_thread_watchdog(void __attribute__((unused)) *arg)
{
	time_t log_t = 0, up_t = 0;
	for (;;) {
		time_t t = time(0);
		if (t - up_t >= UPDATE_PERIOD) {
			int count = 0;
			up_t = t;
			dnet_traverse_connections(&dnet_conn_count_callback, &count);
			dnet_log_watchdog(count);
		}
		if (t - log_t >= LOG_PERIOD) {
			log_t = t;
			dnet_log_periodic();
		}
		sleep(1);
	}
	return 0;
}

int dnet_thread_create(struct dnet_thread *t)
{
	void *(*run)(void *);
	int res;
	switch (t->type) {
	case DNET_THREAD_FORWARD_TO:
		t->type = DNET_THREAD_STREAM;
	case DNET_THREAD_CLIENT:
	case DNET_THREAD_SERVER:
	case DNET_THREAD_FORWARD_FROM:
		run = &dnet_thread_client_server;
		break;
	case DNET_THREAD_ACCEPTED:
		run = &dnet_thread_accepted;
		break;
	case DNET_THREAD_STREAM:
		run = &dnet_thread_stream;
		break;
	case DNET_THREAD_EXCHANGER:
		run = &dnet_thread_exchanger;
		break;
	case DNET_THREAD_WATCHDOG:
		run = &dnet_thread_watchdog;
		break;
	case DNET_THREAD_COLLECTOR:
		run = &dnet_thread_collector;
		break;
	default:
		return -1;
	}
	t->nthread = g_nthread++;
	t->conn.sess = 0;
	t->to_remove = 0;
	t->id = pthread_invalid;
	dthread_mutex_init(&t->conn.mutex, 0);
	pthread_rwlock_wrlock(&g_threads_rwlock);
	list_insert_before(g_threads, &t->threads);
	pthread_rwlock_unlock(&g_threads_rwlock);
	res = pthread_create(&t->id, NULL, run, t);
	if (res) t->id = pthread_invalid;
	else pthread_detach(t->id);
	return res;
}

int dnet_threads_init(void)
{
	g_threads = malloc(sizeof(struct list));
	if (!g_threads) return -1;
	list_init(g_threads);
	pthread_rwlock_init(&g_threads_rwlock, 0);
	g_nthread = 0;
	return 0;
}
