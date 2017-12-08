/* база хостов, T13.714-T13.716 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../ldus/source/include/ldus/rbtree.h"
#include "transport.h"
#include "netdb.h"
#include "log.h"
#include "main.h"
#include "block.h"

#define MAX_SELECTED_HOSTS	64
#define DATABASE			(g_cheatcoin_testnet ? "netdb-testnet.txt" : "netdb.txt")

enum host_flags {
	HOST_OUR		= 1,
	HOST_CONNECTED	= 2,
	HOST_SET		= 4,
	HOST_INDB		= 8,
	HOST_NOT_ADD	= 0x10,
};

struct host {
	struct ldus_rbtree node;
	uint32_t ip;
	uint16_t port;
	uint16_t flags;
};

static inline int lessthen(struct ldus_rbtree *l, struct ldus_rbtree *r) {
	return memcmp(l + 1, r + 1, 6) < 0;
}

ldus_rbtree_define_prefix(lessthen, static inline, )

static struct ldus_rbtree *root = 0;
static pthread_mutex_t host_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct host *selected_hosts[MAX_SELECTED_HOSTS], *our_host;
static unsigned n_selected_hosts = 0;

static struct host *find_add_host(struct host *h) {
	struct host *h0;
	pthread_mutex_lock(&host_mutex);
	if ((h0 = (struct host *)ldus_rbtree_find(root, &h->node))) {
		if (h->flags & HOST_SET) h0->flags = h->flags;
	} else if (!(h->flags & HOST_NOT_ADD) && (h0 = malloc(sizeof(struct host)))) {
		memcpy(h0, h, sizeof(struct host));
		ldus_rbtree_insert(&root, &h0->node);
		g_cheatcoin_stats.nhosts++;
		if (g_cheatcoin_stats.nhosts > g_cheatcoin_stats.total_nhosts)
			g_cheatcoin_stats.total_nhosts = g_cheatcoin_stats.nhosts;
		if (!(h->flags & HOST_INDB)) {
			FILE *f = fopen(DATABASE, "a");
			if (f) {
				fprintf(f, "%u.%u.%u.%u:%u\n", h->ip & 0xff, h->ip >> 8 & 0xff, h->ip >> 16 & 0xff, h->ip >> 24 & 0xff, h->port);
				fclose(f);
			}
		}
	}
	pthread_mutex_unlock(&host_mutex);
	return h0;
}

static struct host *random_host(int mask) {
	struct ldus_rbtree *r, *p = 0;
	int i, n;
	for (i = 0; !p && i < 10; ++i) {
		pthread_mutex_lock(&host_mutex);
		r = root, p = 0;
		n = g_cheatcoin_stats.nhosts;
		while (r) {
			p = _rbtree_ptr(r);
			if (!(rand() % (n > 3 ? n : 3))) break;
			r = (rand() & 1 ? p->left : p->right);
			n >>= 1;
		}
		if (p && ((struct host *)p)->flags & mask) p = 0;
		pthread_mutex_unlock(&host_mutex);
	}
	return (struct host *)p;
}

static struct host *ipport2host(const char *ipport, int flags) {
	static struct host h; uint8_t ip[4];
	if (sscanf(ipport, "%hhu.%hhu.%hhu.%hhu:%hu", ip, ip + 1, ip + 2, ip + 3, &h.port) != 5) return 0;
	memcpy(&h.ip, ip, 4);
	h.flags = flags;
	return &h;
}

static struct host *find_add_ipport(const char *ipport, int flags) {
	struct host *h;
	if (!ipport) return 0;
	h = ipport2host(ipport, flags);
	if (!h) return 0;
	return find_add_host(h);
}

static int read_database(const char *fname, int flags) {
	struct host *h;
	char str[64], *p;
	FILE *f = fopen(fname, "r");
	int n = 0;
	if (!f) return 0;
	while(fscanf(f, "%s", str) == 1) {
		p = strchr(str, ':');
		if (!p || !p[1]) continue;
		h = find_add_ipport(str, flags);
		if (!h) continue;
		if (flags & HOST_CONNECTED && n_selected_hosts < MAX_SELECTED_HOSTS / 2) selected_hosts[n_selected_hosts++] = h;
		n++;
	}
	fclose(f);
	return n;
}

static void reset_callback(struct ldus_rbtree *node) {
	struct host *h = (struct host *)node;
	h->flags &= ~HOST_CONNECTED;
}

static void *monitor_thread(void *arg) {
	for(;;) {
		FILE *f = fopen("netdb.tmp", "w");
		int n, i;
		if (!f) continue;
		cheatcoin_net_command("conn", f);
		fclose(f);
		pthread_mutex_lock(&host_mutex);
		ldus_rbtree_walk_right(root, reset_callback);
		pthread_mutex_unlock(&host_mutex);
		n_selected_hosts = 0;
		if (our_host) selected_hosts[n_selected_hosts++] = our_host;
		n = read_database("netdb.tmp", HOST_CONNECTED | HOST_SET | HOST_NOT_ADD);
		for (i = 0; i < MAX_SELECTED_HOSTS; ++i) {
			struct host *h = random_host(HOST_CONNECTED | HOST_OUR);
			char str[64];
			if (!h) continue;
			h->flags |= HOST_CONNECTED;
			if (n < MAX_SELECTED_HOSTS) {
				sprintf(str, "connect %u.%u.%u.%u:%u", h->ip & 0xff, h->ip >> 8 & 0xff, h->ip >> 16 & 0xff, h->ip >> 24 & 0xff, h->port);
				cheatcoin_net_command(str, stderr);
				n++;
			}
			if (n_selected_hosts < MAX_SELECTED_HOSTS) selected_hosts[n_selected_hosts++] = h;
		}
		sleep(67);
	}
	return 0;
}

/* инициализировать базу хостов; our_host - внешний адрес нашего хоста (ip:port), addr_port_pairs - адреса других npairs хостов в том же формате */
int cheatcoin_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs) {
	pthread_t t;
	int i;
	read_database(DATABASE, HOST_INDB);
	our_host = find_add_ipport(our_host_str, HOST_OUR | HOST_SET);
	for (i = 0; i < npairs; ++i) find_add_ipport(addr_port_pairs[i], 0);
	if (pthread_create(&t, 0, monitor_thread, 0)) { cheatcoin_fatal("Can't start netdb thread\n"); return -1; }
	return 0;
}

/* записывает в массив данные для передачи другому хосту */
unsigned cheatcoin_netdb_send(uint8_t *data, unsigned len) {
	unsigned i;
	for (i = 0; i < n_selected_hosts && len >= 6; ++i, len -= 6, data += 6)
		memcpy(data, &selected_hosts[i]->ip, 6);
	memset(data, 0, len);
	return i * 6;
}

/* читает данные, переданные другим хостом */
unsigned cheatcoin_netdb_receive(const uint8_t *data, unsigned len) {
	struct host h;
	int i;
	h.flags = 0;
	for (i = 0; len >= 6; ++i, len -= 6, data += 6) {
		memcpy(&h.ip, data, 6);
		if (!h.ip || !h.port) continue;
		find_add_host(&h);
	}
	return 6 * i;
}
