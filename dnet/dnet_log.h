/* dnet: log; T11.261-T13.063; $DVS:time$ */

#ifndef DNET_LOG_H_INCLUDED
#define DNET_LOG_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include "system.h"

struct dnet_output {
    FILE *f;
    char *str;
    size_t len;
    void *data;
    void (*callback)(struct dnet_output *out);
    int count;
    int err;
};

#define DNET_ACTIVE_PERIOD	    300

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dnet_printf(struct dnet_output *out, const char *format, ...);
extern ssize_t dnet_write(struct dnet_output *out, const void *data, size_t size);
extern void dnet_print_hosts(struct dnet_output *out, long active_time);
extern int dnet_print_connections(struct dnet_output *out);
extern void dnet_print_streams(struct dnet_output *out);
extern void dnet_log_periodic(void);
extern int dnet_log_printf(const char *format, ...);
extern void dnet_log_watchdog(int count);

#ifdef __cplusplus
};
#endif	

#endif
