/* dnet: crypt; T11.231-T12.997; $DVS:time$ */

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

#include "dnet_database.h"

struct dnet_session;

struct dnet_session_ops {
    ssize_t (*read)(void *private_data, void *buf, size_t size);
    ssize_t (*write)(void *private_data, void *buf, size_t size);
};

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dnet_limited_version;

extern int dnet_crypt_init(const char *version);

extern struct dnet_session *dnet_session_create(void *private_data, const struct dnet_session_ops *ops, uint32_t route_ip, uint16_t route_port);
extern int dnet_session_init(struct dnet_session *sess);
extern ssize_t dnet_session_write(struct dnet_session *sess, void *buf, size_t size);
extern ssize_t dnet_session_read(struct dnet_session *sess, void *buf, size_t size);
extern struct dnet_host *dnet_session_get_host(struct dnet_session *sess);
extern void dnet_generate_stream_id(struct dnet_stream_id *id);

#ifdef __cplusplus
};
#endif

#endif


