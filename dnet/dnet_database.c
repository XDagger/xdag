/* dnet: database; T11.231-T13.808; $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include "../dus/programs/dar/source/include/crc.h"
#include "dnet_database.h"
#include "dnet_main.h"
#include "../utils/utils.h"

#define DNET_HOST_MAX           0x1000
#define DNET_NEW_HOST_TIMEOUT	DNET_ACTIVE_PERIOD
#define DNET_AUTO_ROUTE_TIMEOUT	10
#define KEYS_FILE				"dnet_keys.dat"
#define NAME_FILE				"dnet_name.txt"

static pthread_mutex_t g_host_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct dnet_host *g_dnet_hosts;
static unsigned g_dnet_n_hosts;

void dnet_update_host(struct dnet_host *host, time_t active_ago, uint32_t route_ip, uint16_t route_port, enum dnet_host_route_types route_type) {
	time_t now = time(0);
	time_t t = now - active_ago;
	if (t > host->last_active + (active_ago >> 7)) {
		if (t - host->last_active >= DNET_NEW_HOST_TIMEOUT) host->last_appear = now;
		if (route_type >= host->route_type) {
			if (route_type != DNET_ROUTE_AUTO || t - host->last_active >= DNET_AUTO_ROUTE_TIMEOUT) host->route_ip = route_ip, host->route_port = route_port;
            host->route_type = route_type;
        }
		host->last_active = t;
		host->last_time_changed = now;
    }
}

struct dnet_host *dnet_add_host(const struct dnet_key *key, time_t active_ago, uint32_t route_ip, uint16_t route_port, enum dnet_host_route_types route_type) {
    struct dnet_host *host;
	time_t now;
	unsigned i, n;
begin:
	n = g_dnet_n_hosts;
	for (i = 0; i < n; ++i) {
		host = &g_dnet_hosts[i];
		if (!memcmp(key, host->key.key, sizeof(struct dnet_key))) {
			dnet_update_host(host, active_ago, route_ip, route_port, route_type);
			return host;
		}
	}
	if (n == DNET_HOST_MAX) return 0;
	pthread_mutex_lock(&g_host_mutex);
	if (n != g_dnet_n_hosts) {
		pthread_mutex_unlock(&g_host_mutex);
		goto begin;
	}
	host = &g_dnet_hosts[n];
    memset(host, 0, sizeof(struct dnet_host));
    memcpy(host->key.key, key, sizeof(struct dnet_key));
    host->crc32 = crc_of_array((unsigned char *)key, sizeof(struct dnet_key));
	now = time(0);
	host->last_active = now - active_ago;
	host->last_time_changed = now;
	host->last_appear = now;
    host->route_ip = route_ip;
	host->route_port = route_port;
	host->route_type = route_type;
	if (!n) {
		FILE *f = xdag_open_file(NAME_FILE, "rb");
		if (f) {
			int len = fread(host->name, 1, DNET_HOST_NAME_MAX, f);
			xdag_close_file(f);
			if (len > 0) host->name_len = len;
		}
		host->is_local = 1;
		host->is_trusted = 1;
	} else {
		FILE *f = xdag_open_file(KEYS_FILE, "rb");
		if (!f) host->is_trusted = 1;
		else {
			struct dnet_key k;
			while (fread(&k, sizeof(struct dnet_key), 1, f) == 1) {
				if (!memcmp(key, &k, sizeof(struct dnet_key))) {
					host->is_trusted = 1;
					break;
				}
			}
			xdag_close_file(f);
		}
	}
	g_dnet_n_hosts++;
	pthread_mutex_unlock(&g_host_mutex);
	return host;
}

struct dnet_host *dnet_get_self_host(void) {
    return g_dnet_hosts;
}

struct dnet_host *dnet_get_host_by_crc(uint32_t crc) {
    unsigned i;
    for (i = 0; i < g_dnet_n_hosts; ++i) {
        struct dnet_host *host = &g_dnet_hosts[i];
        if (host->crc32 == crc) return host;
    }
    return 0;
}

struct dnet_host *dnet_get_host_by_name(const char *name_or_crc) {
    uint32_t crc;
    unsigned len = strlen(name_or_crc), i;
    if (!len) return 0;
    for (i = 0; i < g_dnet_n_hosts; ++i) {
        struct dnet_host *host = &g_dnet_hosts[i];
        if (len == host->name_len && !memcmp(name_or_crc, host->name, host->name_len)) return host;
    }
    if (sscanf(name_or_crc, "%x", &crc) != 1) return 0;
    return dnet_get_host_by_crc(crc);
}

static int dnet_hosts_compar_t(struct dnet_host *l, struct dnet_host *r, uint32_t active_time) {
	time_t t = time(0);
	uint32_t tl = dnet_host_time_ago(l, t), tr;
	if (!r) return tl <= active_time ? 0 : -2;
	if (l->is_trusted != r->is_trusted) return r->is_trusted - l->is_trusted;
	if (l->route_type != r->route_type) return r->route_type - l->route_type;
	tr = dnet_host_time_ago(r, t);
	if (tl > DNET_ACTIVE_PERIOD || tr > DNET_ACTIVE_PERIOD) return tl < tr ? -1 : 1;
	return l < r ? -1 : 1;
}

int dnet_hosts_compar(struct dnet_host **l, struct dnet_host **r) {
	return dnet_hosts_compar_t(*l, *r, DNET_ACTIVE_PERIOD);
}

int dnet_hosts_compar_hour(struct dnet_host **l, struct dnet_host **r) {
	return dnet_hosts_compar_t(*l, *r, 3600);
}

int dnet_hosts_compar_day(struct dnet_host **l, struct dnet_host **r) {
	return dnet_hosts_compar_t(*l, *r, 3600 * 24);
}

int dnet_hosts_compar_all(struct dnet_host **l, struct dnet_host **r) {
	return dnet_hosts_compar_t(*l, *r, UINT_MAX);
}

int dnet_traverse_hosts(int (*callback)(struct dnet_host *host, void *data), void *data) {
	int n = g_dnet_n_hosts, res = 0, i;
	for (i = 0; i < n; ++i) {
		res = (*callback)(&g_dnet_hosts[i], data);
		if (res) break;
	}
	return res;
}

int dnet_traverse_filtered_hosts(int (*callback)(struct dnet_host *host, void *data), int (*compar)(struct dnet_host **l, struct dnet_host **r), void *data) {
	struct dnet_host *hostrefs[DNET_HOST_MAX];
	int n = g_dnet_n_hosts, res = 0, i, j;
	for (i = j = 0; i < n; ++i) {
		struct dnet_host *h = &g_dnet_hosts[i], *h0 = 0;
		if (!compar(&h, &h0))
			hostrefs[j++] = h;
	}
	n = j;
	qsort(hostrefs, n, sizeof(struct dnet_host *), (int (*)(const void *, const void *))compar);
	for (i = 0; i < n; ++i) {
		res = (*callback)(hostrefs[i], data);
        if (res) break;
    }
    return res;
}

int dnet_trust_host(struct dnet_host *host) {
	FILE *f;
    unsigned i;
	if (host == &g_dnet_hosts[0]) return 0;
	if (!(f = xdag_open_file(KEYS_FILE, "rb"))) {
		f = xdag_open_file(KEYS_FILE, "wb");
		if (!f) return 1;
		for (i = 1; i < g_dnet_n_hosts; ++i) {
			g_dnet_hosts[i].is_trusted = 0;
		}
	}
	xdag_close_file(f);
	if (host->is_trusted) return 0;
	f = xdag_open_file(KEYS_FILE, "ab");
	if (!f) return 2;
	if (fwrite(&host->key, sizeof(struct dnet_key), 1, f) != 1) {
		xdag_close_file(f);
		return 3;
	}
	xdag_close_file(f);
	host->is_trusted = 1;
	return 0;
}

int dnet_untrust_host(struct dnet_host *host) {
	return -1;
}

int dnet_set_host_name(struct dnet_host *host, const char *name, size_t len) {
    unsigned i;
    if (host->name_len == len && !memcmp(host->name, name, len)) return 0;
	for (i = 0; i < g_dnet_n_hosts; ++i) {
		struct dnet_host *host1 = &g_dnet_hosts[i];
		if (len == host1->name_len && !memcmp(name, host1->name, host1->name_len)) return 1;
	}
	host->last_appear = time(0);
    memcpy(host->name, name, len);
    host->name_len = len;
	if (host == g_dnet_hosts) {
		FILE *f = xdag_open_file(NAME_FILE, "wb");
		if (f) {
			fwrite(host->name, 1, host->name_len, f);
			xdag_close_file(f);
		}
	}
	return 0;
}

int dnet_set_host_version(struct dnet_host *host, const char *version) {
	if (strlen(version) >= sizeof(host->version)) return -1;
	if (!strcmp(host->version, version)) return 0;
	if (host == g_dnet_hosts && host->version[0]) return -2;
	host->last_appear = time(0);
	strcpy(host->version, version);
	return 0;
}

int dnet_set_self_version(const char *version) {
	if (!g_dnet_n_hosts || strlen(version) >= sizeof(g_dnet_hosts->version)) return -1;
	if (!strcmp(g_dnet_hosts->version, version)) return 0;
	g_dnet_hosts->last_appear = time(0);
	strcpy(g_dnet_hosts->version, version);
	return 0;
}

static const char *get_host_version(struct dnet_host *host) {
	if (host->version[0]) return host->version;
	else return "unknown";
}

int dnet_print_host_name(struct dnet_host *host, struct dnet_output *out) {
    if (host->name_len) dnet_printf(out, "%-15.*s", host->name_len, host->name);
	else dnet_printf(out, "%08X       ", host->crc32);
    return 15;
}

int dnet_print_host_brief(struct dnet_host *host, struct dnet_output *out) {
	int len = dnet_print_host_name(host, out);
	dnet_printf(out, "%*u sec, %s%s\n", 28 - len, dnet_host_time_ago(host, time(0)),
			get_host_version(host), (host->is_trusted ? ", trust" : ""));
	return 0;
}

int dnet_print_host(struct dnet_host *host, struct dnet_output *out) {
    const char *route_type; int len;
    dnet_printf(out, " %2d. ", out->count);
    len = dnet_print_host_name(host, out);
    switch (host->route_type) {
		case DNET_ROUTE_NONE:       route_type = "none";        break;
		case DNET_ROUTE_AUTO:       route_type = "auto";        break;
		case DNET_ROUTE_IMMEDIATE:  route_type = "immediate";   break;
		case DNET_ROUTE_LOCAL:      route_type = "local";       break;
		case DNET_ROUTE_MANUAL:     route_type = "manual";      break;
		default: return -1;
    }
	dnet_printf(out, "%*u sec, %s, %-10s%5s  %d.%d.%d.%d:%d\n", 28 - len, dnet_host_time_ago(host, time(0)),
		get_host_version(host), route_type, (host->is_trusted ? "trust" : ""),
		host->route_ip >> 24 & 0xFF, host->route_ip >> 16 & 0xFF, host->route_ip >> 8 & 0xFF, host->route_ip & 0xFF, host->route_port);
    out->count++;
    return 0;
}

int dnet_hosts_init(void) {
    g_dnet_hosts = malloc(DNET_HOST_MAX * sizeof(struct dnet_host));
    if (!g_dnet_hosts) return -1;
    g_dnet_n_hosts = 0;
    return 0;
}
