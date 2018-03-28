/* база хостов, T13.714-T13.841 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "system.h"
#include "../ldus/source/include/ldus/rbtree.h"
#include "transport.h"
#include "netdb.h"
#include "log.h"
#include "init.h"
#include "block.h"
#include "sync.h"
#include "../utils/utils.h"

#define MAX_SELECTED_HOSTS  64
#define MAX_BLOCKED_IPS     64
#define MAX_WHITE_IPS       64
#define MAX_ALLOWED_FROM_IP 4
#define DATABASE            (g_xdag_testnet ? "netdb-testnet.txt" : "netdb.txt")
#define DATABASEWHITE       (g_xdag_testnet ? "netdb-white-testnet.txt" : "netdb-white.txt")

enum host_flags {
	HOST_OUR        = 1,
	HOST_CONNECTED  = 2,
	HOST_SET        = 4,
	HOST_INDB       = 8,
	HOST_NOT_ADD    = 0x10,
	HOST_WHITE      = 0x20,
};

struct host {
	struct ldus_rbtree node;
	uint32_t ip;
	uint16_t port;
	uint16_t flags;
};

static inline int lessthan(struct ldus_rbtree *l, struct ldus_rbtree *r)
{
	struct host *lh = (struct host *)l, *rh = (struct host *)r;

	return lh->ip < rh->ip || (lh->ip == rh->ip && lh->port < rh->port);
}

ldus_rbtree_define_prefix(lessthan, static inline, )

static struct ldus_rbtree *root = 0;
static pthread_mutex_t host_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct host *selected_hosts[MAX_SELECTED_HOSTS], *our_host;
static unsigned n_selected_hosts = 0;

/* blocked ip for incoming connections and their number */
uint32_t *g_xdag_blocked_ips, *g_xdag_white_ips;
int g_xdag_n_blocked_ips = 0, g_xdag_n_white_ips = 0;

static struct host *find_add_host(struct host *h)
{
	struct host *h0;

	pthread_mutex_lock(&host_mutex);

	if ((h0 = (struct host *)ldus_rbtree_find(root, &h->node))) {
		if (h->flags & HOST_SET) h0->flags = h->flags;
	} else if (!(h->flags & HOST_NOT_ADD) && (h0 = malloc(sizeof(struct host)))) {
		memcpy(h0, h, sizeof(struct host));
		ldus_rbtree_insert(&root, &h0->node);
		g_xdag_stats.nhosts++;
		
		if (g_xdag_stats.nhosts > g_xdag_stats.total_nhosts) {
			g_xdag_stats.total_nhosts = g_xdag_stats.nhosts;
		}

		if (!(h->flags & HOST_INDB)) {
			FILE *f = xdag_open_file(DATABASE, "a");
			if (f) {
				fprintf(f, "%u.%u.%u.%u:%u\n", h->ip & 0xff, h->ip >> 8 & 0xff, h->ip >> 16 & 0xff, h->ip >> 24 & 0xff, h->port);
				xdag_close_file(f);
			}
		}
	}

	if (h->flags & HOST_WHITE) {
		if (g_xdag_n_white_ips < MAX_WHITE_IPS) {
			g_xdag_white_ips[g_xdag_n_white_ips++] = h0->ip;
		}
	}

	pthread_mutex_unlock(&host_mutex);
	
	return h0;
}

static struct host *random_host(int mask)
{
	struct ldus_rbtree *r, *p = 0;
	int i, n;

	for (i = 0; !p && i < 10; ++i) {
		pthread_mutex_lock(&host_mutex);

		r = root, p = 0;
		n = g_xdag_stats.nhosts;

		while (r) {
			p = _rbtree_ptr(r);
			if (!(rand() % (n > 3 ? n : 3))) break;
			r = (rand() & 1 ? p->left : p->right);
			n >>= 1;
		}

		if (p && (((struct host *)p)->flags & mask)) p = 0;

		pthread_mutex_unlock(&host_mutex);
	}

	return (struct host *)p;
}

static struct host *ipport2host(struct host *h, const char *ipport, int flags)
{
	unsigned ip[5];

	if (sscanf(ipport, "%u.%u.%u.%u:%u", ip, ip + 1, ip + 2, ip + 3, ip + 4) != 5
		|| (ip[0] | ip[1] | ip[2] | ip[3]) & ~0xff || ip[4] & ~0xffff) return 0;

	h->ip = ip[0] | ip[1] << 8 | ip[2] << 16 | ip[3] << 24;
	h->port = ip[4];
	h->flags = flags;

	return h;
}

static struct host *find_add_ipport(struct host *h, const char *ipport, int flags)
{
	if (!ipport) return 0;
	
	h = ipport2host(h, ipport, flags);
	
	if (!h) return 0;
	
	return find_add_host(h);
}

static int read_database(const char *fname, int flags)
{
	uint32_t ips[MAX_BLOCKED_IPS * MAX_ALLOWED_FROM_IP];
	uint8_t ips_count[MAX_BLOCKED_IPS * MAX_ALLOWED_FROM_IP];
	struct host h0, *h;
	char str[64], *p;
	FILE *f = xdag_open_file(fname, "r");
	int n = 0, n_ips = 0, n_blocked = 0, i;

	if (!f) return -1;

	while (fscanf(f, "%63s", str) == 1) {
		p = strchr(str, ':');
		if (!p || !p[1]) continue;
	
		h0.flags = 0;
		h = find_add_ipport(&h0, str, flags);
		
		if (flags & HOST_CONNECTED && h0.flags & HOST_CONNECTED) {
			for (i = 0; i < n_ips && ips[i] != h0.ip; ++i);

			if (i == n_ips && i < MAX_BLOCKED_IPS * MAX_ALLOWED_FROM_IP) {
				ips[i] = h0.ip, ips_count[i] = 1, n_ips++;
			} else if (i < n_ips && ips_count[i] < MAX_ALLOWED_FROM_IP
				       && ++ips_count[i] == MAX_ALLOWED_FROM_IP && n_blocked < MAX_BLOCKED_IPS) {
				g_xdag_blocked_ips[n_blocked++] = ips[i];
			}
		}

		if (!h) continue;

		xdag_debug("Netdb : host=%lx, flags=%x, read '%s'", (long)h, h->flags, str);
		
		if (flags & HOST_CONNECTED && n_selected_hosts < MAX_SELECTED_HOSTS / 2) selected_hosts[n_selected_hosts++] = h;
		
		n++;
	}

	xdag_close_file(f);
	
	if (flags & HOST_CONNECTED) g_xdag_n_blocked_ips = n_blocked;
	
	return n;
}

static void reset_callback(struct ldus_rbtree *node)
{
	struct host *h = (struct host *)node;

	h->flags &= ~HOST_CONNECTED;
}

static void *monitor_thread(void *arg)
{
	while (!g_xdag_sync_on) {
		sleep(1);
	}

	for (;;) {
		FILE *f = xdag_open_file("netdb.tmp", "w");
		int n, i, j;
		time_t t = time(0);

		if (!f) continue;

		xdag_net_command("conn", f);
		
		xdag_close_file(f);
		
		pthread_mutex_lock(&host_mutex);
		
		ldus_rbtree_walk_right(root, reset_callback);
		
		pthread_mutex_unlock(&host_mutex);
		
		n_selected_hosts = 0;
		
		if (our_host)
			selected_hosts[n_selected_hosts++] = our_host;

		n = read_database("netdb.tmp", HOST_CONNECTED | HOST_SET | HOST_NOT_ADD);
		if (n < 0) n = 0;

		f = xdag_open_file("netdb.log", "a");

		for (i = 0; i < MAX_SELECTED_HOSTS; ++i) {
			struct host *h = random_host(HOST_CONNECTED | HOST_OUR);
			char str[64];

			if (!h) continue;

			if (n < MAX_SELECTED_HOSTS) {
				for (j = 0; j < g_xdag_n_white_ips; ++j) {
					if (h->ip == g_xdag_white_ips[j]) {
						sprintf(str, "connect %u.%u.%u.%u:%u", h->ip & 0xff, h->ip >> 8 & 0xff, h->ip >> 16 & 0xff, h->ip >> 24 & 0xff, h->port);
						xdag_debug("Netdb : host=%lx flags=%x query='%s'", (long)h, h->flags, str);
						xdag_net_command(str, (f ? f : stderr));
						n++;
						break;
					}
				}
			}

			h->flags |= HOST_CONNECTED;
			
			if (n_selected_hosts < MAX_SELECTED_HOSTS) selected_hosts[n_selected_hosts++] = h;
		}

		if (f) xdag_close_file(f);
		
		g_xdag_n_white_ips = 0;
		
		read_database(DATABASEWHITE, HOST_WHITE);

		while (time(0) - t < 67) {
			sleep(1);
		}
	}

	return 0;
}

/* initialized hosts base, 'our_host_str' - exteranal address of our host (ip:port),
* 'addr_port_pairs' - addresses of other 'npairs' hosts in the same format
*/
int xdag_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs)
{
	struct host h;
	pthread_t t;
	int i;

	g_xdag_blocked_ips = malloc(MAX_BLOCKED_IPS * sizeof(uint32_t));
	g_xdag_white_ips = malloc(MAX_WHITE_IPS * sizeof(uint32_t));
	
	if (!g_xdag_blocked_ips || !g_xdag_white_ips) return -1;
	
	if (read_database(DATABASE, HOST_INDB) < 0) {
		xdag_fatal("Can't find file '%s'\n", DATABASE); return -1;
	}
	
	read_database(DATABASEWHITE, HOST_WHITE);
	
	our_host = find_add_ipport(&h, our_host_str, HOST_OUR | HOST_SET);

	for (i = 0; i < npairs; ++i) {
		find_add_ipport(&h, addr_port_pairs[i], 0);
	}
	
	if (pthread_create(&t, 0, monitor_thread, 0)) {
		xdag_fatal("Can't start netdb thread\n"); return -1;
	}
	
	return 0;
}

/* writes data to the array for transmission to another host */
unsigned xdag_netdb_send(uint8_t *data, unsigned len)
{
	unsigned i;

	for (i = 0; i < n_selected_hosts && len >= 6; ++i, len -= 6, data += 6) {
		memcpy(data, &selected_hosts[i]->ip, 4);
		memcpy(data + 4, &selected_hosts[i]->port, 2);
	}
	
	memset(data, 0, len);
	
	return i * 6;
}

/* reads data sent by another host */
unsigned xdag_netdb_receive(const uint8_t *data, unsigned len)
{
	struct host h;
	int i;

	h.flags = 0;

	for (i = 0; len >= 6; ++i, len -= 6, data += 6) {
		memcpy(&h.ip, data, 4);
		memcpy(&h.port, data + 4, 2);
		if (!h.ip || !h.port) continue;
		find_add_host(&h);
	}
	
	return 6 * i;
}

/* completes the work with the host database */
void xdag_netdb_finish(void)
{
	pthread_mutex_lock(&host_mutex);
}
