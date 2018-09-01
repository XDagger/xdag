/* dnet: packets; T11.258-T14.290; $DVS:time$ */

#ifndef DNET_PACKET_H_INCLUDED
#define DNET_PACKET_H_INCLUDED

#include <stdint.h>

#define DNET_PKT_STREAM_DATA_MAX   0x1000

enum dnet_packet_types {
	DNET_PKT_MIN			= 0x80,
    DNET_PKT_EXCHANGE	    = 0x80,
	DNET_PKT_UPDATE			= 0x81,
    DNET_PKT_COMMAND	    = 0x82,
    DNET_PKT_COMMAND_OUTPUT = 0x83,
    DNET_PKT_SHELL_INPUT    = 0x84,
    DNET_PKT_SHELL_OUTPUT   = 0x85,
	DNET_PKT_TUNNELED_MSG	= 0x86,
	DNET_PKT_FORWARDED_TCP	= 0x87,
	DNET_PKT_FORWARDED_UDP	= 0x88,
	DNET_PKT_FILE_OP		= 0x89,
	DNET_PKT_CRYPT			= 0x8A,
	DNET_PKT_XDAG		= 0x8B,
	DNET_PKT_MAX			= 0x8B,
};

struct dnet_packet_header {
    uint8_t type;
    uint8_t ttl;
    uint16_t length;
    uint32_t crc32;
};

#ifdef __cplusplus
};
#endif

#endif
