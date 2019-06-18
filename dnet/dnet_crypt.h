/* dnet: crypt; T11.231-T14.290; $DVS:time$ */

#pragma once

#include <sys/types.h>

#include "../dfslib/dfsrsa.h"

#include "dnet_system.h"

#define DNET_KEY_SIZE	4096
#define DNET_KEYLEN	((DNET_KEY_SIZE * 2) / (sizeof(dfsrsa_t) * 8))

#define CRYPT_SECTOR_LOG  9
#define CRYPT_SECTOR_SIZE (1 << CRYPT_SECTOR_LOG)

struct dnet_key
{
	dfsrsa_t key[DNET_KEYLEN];
};

struct dnet_stream_id
{
	uint32_t id[4];
};

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_crypt_init(void);

extern void dnet_generate_stream_id(struct dnet_stream_id *id);
extern void dnet_session_init_crypt(struct dfslib_crypt *crypt, uint32_t sector[CRYPT_SECTOR_SIZE / 4]);

#ifdef __cplusplus
};
#endif
