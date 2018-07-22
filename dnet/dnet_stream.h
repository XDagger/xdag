/* dnet: streams; T11.270-T13.426; $DVS:time$ */

#ifndef DNET_STREAM_H_INCLUDED
#define DNET_STREAM_H_INCLUDED

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include "system.h"
#include "dthread.h"
#include "dnet_packet.h"
#include "dnet_log.h"

struct dnet_tty_params {
    uint16_t xwinsize, ywinsize;
};

struct dnet_ipport {
	uint32_t ip;
	uint16_t port;
} __attribute__((packed));

enum dnet_proto {
	DNET_TCP,
	DNET_UDP,
};

struct dnet_stream {
	dthread_mutex_t mutex;
    void *sess_reserved;
	int input_tty;
	int output_tty;
	uint32_t ip_to;
	uint16_t port_to;
	uint16_t port_from;
	struct dnet_stream_id id;
    uint64_t seq, ack;
	const char *file_from, *file_to, *file_param;
    struct dnet_tty_params tty_params;
    uint32_t crc_from, crc_to;
    time_t creation_time;
    pid_t child;
	int tap_from;
	int tap_to;
	uint8_t pkt_type;
	uint8_t to_reinit;
    uint8_t to_exit;
	uint8_t proto;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_send_stream_packet(struct dnet_packet_stream *st, struct dnet_connection *conn);
extern void *dnet_thread_stream(void *arg);
extern int dnet_stream_main(uint32_t crc_to);
extern int dnet_stream_create_tunnel(uint32_t crc_to, int tap_from, int tap_to, struct dnet_stream_id *id);
extern int dnet_stream_forward(uint32_t crc_to, int proto, int port_from, uint32_t ip_to, int port_to);
extern int dnet_traverse_streams(int (*callback)(struct dnet_stream *st, void *data), void *data);
extern int dnet_print_stream(struct dnet_stream *st, struct dnet_output *out);
extern int dnet_process_stream_packet(struct dnet_packet_stream *p);
extern int dnet_finish_stream(uint16_t id);

#ifdef __cplusplus
};
#endif

#endif
