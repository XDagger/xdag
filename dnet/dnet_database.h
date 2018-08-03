/* dnet: database; T11.231-T13.410; $DVS:time$ */

#ifndef DNET_DATABASE_H_INCLUDED
#define DNET_DATABASE_H_INCLUDED

#include <time.h>
#include <stdint.h>
#include "dnet_crypt.h"
#include "dnet_log.h"

#define DNET_HOST_NAME_MAX 256

enum dnet_host_route_types {
    DNET_ROUTE_NONE,
    DNET_ROUTE_AUTO,
    DNET_ROUTE_IMMEDIATE,
    DNET_ROUTE_LOCAL,
    DNET_ROUTE_MANUAL
};

struct dnet_host {
    struct dnet_key key;
    char name[DNET_HOST_NAME_MAX];
	char version[16];
    time_t last_active;
	time_t last_appear;
	time_t last_time_changed;
	uint32_t crc32;
    uint32_t route_ip;
    unsigned name_len;
	uint16_t route_port;
    uint8_t is_local;
	uint8_t is_trusted;
    enum dnet_host_route_types route_type;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void dnet_update_host(struct dnet_host *host, time_t active_ago, uint32_t route_ip, uint16_t route_port, enum dnet_host_route_types route_type);
extern struct dnet_host *dnet_add_host(const struct dnet_key *key, time_t active_ago, uint32_t route_ip, uint16_t route_port, enum dnet_host_route_types route_type);
extern struct dnet_host *dnet_get_self_host(void);
extern struct dnet_host *dnet_get_host_by_name(const char *name_or_crc);
extern struct dnet_host *dnet_get_host_by_crc(uint32_t crc);
extern int dnet_hosts_compar(struct dnet_host **l, struct dnet_host **r);
extern int dnet_hosts_compar_hour(struct dnet_host **l, struct dnet_host **r);
extern int dnet_hosts_compar_day(struct dnet_host **l, struct dnet_host **r);
extern int dnet_hosts_compar_all(struct dnet_host **l, struct dnet_host **r);
extern int dnet_traverse_hosts(int (*callback)(struct dnet_host *host, void *data), void *data);
extern int dnet_traverse_filtered_hosts(int (*callback)(struct dnet_host *host, void *data), int (*compar)(struct dnet_host **l, struct dnet_host **r), void *data);
extern int dnet_trust_host(struct dnet_host *host);
extern int dnet_untrust_host(struct dnet_host *host);
extern int dnet_set_host_name(struct dnet_host *host, const char *name, size_t len);
extern int dnet_set_host_version(struct dnet_host *host, const char *version);
extern int dnet_print_host_name(struct dnet_host *host, struct dnet_output *out);
extern int dnet_print_host_brief(struct dnet_host *host, struct dnet_output *out);
extern int dnet_print_host(struct dnet_host *host, struct dnet_output *out);
extern int dnet_hosts_init(void);

static inline uint32_t dnet_host_time_ago(struct dnet_host *h, time_t now) {
	return h->is_local || now <= h->last_active ? 0 : (uint32_t)(now - h->last_active);
}
	
#ifdef __cplusplus
};
#endif

#endif
