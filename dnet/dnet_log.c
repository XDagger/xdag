/* dnet: log; T11.261-T13.063; $DVS:time$ */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#ifdef __LDuS__
#include <ldus/system/user.h>
#endif
#include "dnet_database.h"
#include "dnet_connection.h"
#include "dnet_stream.h"
#include "dnet_log.h"
#include "../utils/utils.h"

#define DNET_LOG_PERIOD 300

pthread_mutex_t dnet_log_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t dnet_log_t0 = 0;
int g_connections_count[2];

static int dnet_vprintf(struct dnet_output *out, const char *format, va_list arg) {
    int done;

    if (out->f) done = vfprintf(out->f, format, arg);
    else if (out->str && out->len) {
        done = vsnprintf(out->str, out->len, format, arg);
        if (done >= 0) {
            if ((unsigned)done > out->len) done = out->len;
            out->len -= done;
            out->str += done;
	    if (out->callback) (*out->callback)(out);
        }
	} else done = 0;

    return done;
}

ssize_t dnet_write(struct dnet_output *out, const void *data, size_t size) {
    ssize_t done;

    if (out->f) done = fwrite(data, 1, size, out->f);
    else if (out->str && out->len) {
		done = size;
        if ((size_t)done > out->len) done = out->len;
		memcpy(out->str, data, done);
		out->len -= done;
		out->str += done;
		if (out->callback) (*out->callback)(out);
	} else done = 0;

    return done;
}

int dnet_printf(struct dnet_output *out, const char *format, ...) {
    va_list arg;
	int done;

    va_start (arg, format);
    done = dnet_vprintf(out, format, arg);
    va_end (arg);

    return done;
}

static void dnet_log_open(struct dnet_output *out) {
    time_t t = time(0);
    struct tm tm;
    char tbuf[64];
    localtime_r(&t, &tm);
    strftime(tbuf, 64, "%Y-%m-%d %H:%M:%S  ", &tm);
    pthread_mutex_lock(&dnet_log_mutex);
    out->f = xdag_open_file("dnet.log", "a");
    if (!out->f) out->f = stderr;
    dnet_printf(out, "%s", tbuf);
}

static void dnet_log_close(struct dnet_output *out) {
    if (out->f != stderr) xdag_close_file(out->f);
    out->f = 0;
    pthread_mutex_unlock(&dnet_log_mutex);
}

void dnet_print_hosts(struct dnet_output *out, long active_time) {
    struct dnet_output new_out;
    if (!out) dnet_log_open(out = &new_out);
    out->count = 0;
	dnet_printf(out, "%s hosts:\n", (active_time > 3600*24 ? "All" : active_time > 3600 ? "Daily" : active_time > DNET_ACTIVE_PERIOD ? "Hourly" : "Active"));
	dnet_traverse_filtered_hosts((int (*)(struct dnet_host *, void *))&dnet_print_host,
		(active_time > 3600*24 ? &dnet_hosts_compar_all : active_time > 3600 ? &dnet_hosts_compar_day
		: active_time > DNET_ACTIVE_PERIOD ? &dnet_hosts_compar_hour : &dnet_hosts_compar), out);
    if (out == &new_out) dnet_log_close(out);
}

int dnet_print_connections(struct dnet_output *out) {
    struct dnet_output new_out;
    int count;
    if (!out) dnet_log_open(out = &new_out);
    out->count = 0;
    dnet_printf(out, "Current connections:\n");
    dnet_traverse_connections((int (*)(struct dnet_connection *, void *))&dnet_print_connection, out);
    count = out->count;
    if (out == &new_out) dnet_log_close(out);
    return count;
}

void dnet_print_streams(struct dnet_output *out) {
    struct dnet_output new_out;
    if (!out) dnet_log_open(out = &new_out);
    out->count = 0;
    dnet_printf(out, "Current streams:\n");
    dnet_traverse_streams((int (*)(struct dnet_stream *, void *))&dnet_print_stream, out);
    if (out == &new_out) dnet_log_close(out);
}

void dnet_log_periodic(void) {
#ifndef QDNET
    time_t t = time(0);
    if (t - dnet_log_t0 >= DNET_LOG_PERIOD) {
        dnet_log_t0 = t;
		if (!dnet_limited_version) {
			dnet_print_hosts(0, 0);
			dnet_print_connections(0);
		}
        dnet_print_streams(0);
    }
#endif
}

void dnet_log_watchdog(int count) {
#ifdef __LDuS__
	if (g_connections_count[0] && !g_connections_count[1] && !count) {
		printf("dnet: connection lost\n");
//		ldus_shutdown(0);
	}
#endif
    g_connections_count[0] = g_connections_count[1];
    g_connections_count[1] = count;
}

int dnet_log_printf(const char *format, ...) {
   va_list arg;
   int done;
   struct dnet_output out;

   va_start (arg, format);
   dnet_log_open(&out);
   done = dnet_vprintf(&out, format, arg);
   dnet_log_close(&out);
   va_end (arg);

   return done;
}
