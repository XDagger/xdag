//
//  rpc_service.h
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef XDAG_RPC_SERVICE_H
#define XDAG_RPC_SERVICE_H

#include "cJSON.h"
#include "cJSON_Utils.h"

struct xdag_rpc_connection {
	int fd;
	int pos;
	size_t buffer_size;
	char * buffer;
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* init xdag rpc */
extern int xdag_rpc_service_init(int port);

/* stop xdag rpc */
extern int xdag_rpc_service_stop(void);
	
#ifdef __cplusplus
};
#endif

#endif //XDAG_RPC_SERVICE_H
