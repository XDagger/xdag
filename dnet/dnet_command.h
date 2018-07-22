/* dnet: commands; T11.261-T13.013; $DVS:time$ */

#ifndef DNET_COMMAND_H_INCLUDED
#define DNET_COMMAND_H_INCLUDED

#include "dnet_log.h"
#include "dnet_packet.h"

#define DNET_COMMAND_MAX DNET_PKT_STREAM_DATA_MAX

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_command(const char *in, struct dnet_output *out);

#ifdef __cplusplus
}
#endif

#endif
