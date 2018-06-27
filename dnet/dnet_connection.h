/* dnet: connections; T11.253-T13.093; $DVS:time$ */

#ifndef DNET_CONNECTION_H_INCLUDED
#define DNET_CONNECTION_H_INCLUDED

#include <stdint.h>
#include "dthread.h"
#include "dnet_crypt.h"
#include "dnet_packet.h"
#include "dnet_log.h"

enum dnet_counters {
    DNET_C_IN_BYTES,
    DNET_C_OUT_BYTES,
    DNET_C_IN_PACKETS,
    DNET_C_OUT_PACKETS,
    DNET_C_IN_DROPPED,
    DNET_C_OUT_DROPPED,
    DNET_C_END,
};

#define DNET_CONN_BUF_SIZE  (sizeof(struct dnet_packet))

struct dnet_connection {
	dthread_mutex_t mutex;
    struct dnet_session *sess;
	int socket;
    unsigned buf_pos;
	uint32_t ipaddr;
	uint16_t port;
	uint64_t counters[DNET_C_END];
    char buf[DNET_CONN_BUF_SIZE];
    time_t creation_time;
    time_t last_active_time;
	time_t last_host_exchange_time;
    uint8_t is_new;
};

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dnet_connection_main(struct dnet_connection *conn);
extern int dnet_traverse_connections(int (*callback)(struct dnet_connection *conn, void *data), void *data);
extern int dnet_print_connection(struct dnet_connection *conn, struct dnet_output *out);

#ifdef __cplusplus
};
#endif
		
#endif
