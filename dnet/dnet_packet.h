/* dnet: packets; T11.258-T13.658; $DVS:time$ */

#ifndef DNET_PACKET_H_INCLUDED
#define DNET_PACKET_H_INCLUDED

#include <stdint.h>
#include "dnet_crypt.h"
#include "dnet_database.h"
#include "dnet_log.h"

#define DNET_PKT_STREAM_DATA_MAX   0x1000

enum dnet_packet_types {
	DNET_PKT_MIN		= 0x80,
	DNET_PKT_EXCHANGE	= 0x80,
	DNET_PKT_UPDATE		= 0x81,
	DNET_PKT_XDAG		= 0x8B,
	DNET_PKT_MAX		= 0x8B,
};

struct dnet_packet_header {
    uint8_t type;
    uint8_t ttl;
    uint16_t length;
    uint32_t crc32;
};

struct dnet_packet_exchange {
    struct dnet_packet_header header;
    struct dnet_key pub_key;
    uint32_t time_ago;
    char name[DNET_HOST_NAME_MAX];
};

struct dnet_packet_update_item {
    uint32_t crc32;
    uint32_t time_ago;
};

#define DNET_PKT_EXCHANGE_MIN_LEN ((long)((struct dnet_packet_exchange *)0)->name)
#define DNET_PKT_EXCHANGE_MAX_LEN (DNET_PKT_EXCHANGE_MIN_LEN + DNET_HOST_NAME_MAX)

#define DNET_PKT_UPDATE_ITEMS_MAX (DNET_PKT_STREAM_DATA_MAX / sizeof(struct dnet_packet_update_item) - 1)

struct dnet_packet_update {
    struct dnet_packet_header header;
	struct dnet_packet_update_item item[DNET_PKT_UPDATE_ITEMS_MAX];
};

#define DNET_PKT_UPDATE_MIN_LEN ((long)((struct dnet_packet_update *)0)->item)
#define DNET_PKT_UPDATE_MAX_LEN (DNET_PKT_UPDATE_MIN_LEN + DNET_PKT_UPDATE_ITEMS_MAX * sizeof(struct dnet_packet_update_item))

struct dnet_packet {
    union {
        struct dnet_packet_header header;
        struct dnet_packet_exchange ex;
        struct dnet_packet_update up;
    };
};

struct dnet_connection;

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dnet_process_packet(struct dnet_packet *p, struct dnet_connection *conn);
extern int dnet_send_packet(struct dnet_packet *p, struct dnet_connection *conn);

#ifdef __cplusplus
};
#endif

#endif
