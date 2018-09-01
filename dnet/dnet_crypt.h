/* dnet: crypt; T11.231-T14.290; $DVS:time$ */

#ifndef DNET_CRYPT_H_INCLUDED
#define DNET_CRYPT_H_INCLUDED

#include <sys/types.h>
#include "system.h"
#include "../dus/programs/dfstools/source/include/dfsrsa.h"

#define DNET_KEY_SIZE	4096
#define DNET_KEYLEN	((DNET_KEY_SIZE * 2) / (sizeof(dfsrsa_t) * 8))

struct dnet_key {
    dfsrsa_t key[DNET_KEYLEN];
};

struct dnet_stream_id {
    uint32_t id[4];
};

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dnet_limited_version;

extern int dnet_crypt_init(const char *version);

extern void dnet_generate_stream_id(struct dnet_stream_id *id);

#ifdef __cplusplus
};
#endif

#endif


