/* dnet: threads; T11.231-T13.781; $DVS:time$ */

#ifndef DNET_THREADS_H_INCLUDED
#define DNET_THREADS_H_INCLUDED

#include <stdint.h>
#include <pthread.h>
#include "../ldus/source/include/ldus/list.h"
#include "dnet_connection.h"
#include "dnet_stream.h"

enum dnet_thread_type {
    DNET_THREAD_CLIENT,
    DNET_THREAD_SERVER,
	DNET_THREAD_FORWARD_FROM,
	DNET_THREAD_FORWARD_TO,
	DNET_THREAD_ACCEPTED,
    DNET_THREAD_STREAM,
	DNET_THREAD_EXCHANGER,
	DNET_THREAD_WATCHDOG,
	DNET_THREAD_COLLECTOR,
};

struct dnet_thread {
    struct list threads;
    union {
		struct dnet_connection conn;
		struct dnet_stream st;
    };
    const char *arg;
    pthread_t id;
    int nthread;
    uint8_t to_remove;
    enum dnet_thread_type type;
	char argbuf[32];
};

#define DNET_UPDATE_PERIOD	    30

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_thread_create(struct dnet_thread *t);
extern int dnet_traverse_threads(int (*callback)(struct dnet_thread *t, void *data), void *data);
extern int dnet_threads_init(void);

/* maximum allowed number of connections */
extern int g_conn_limit;

#ifdef __cplusplus
};
#endif

#endif
