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

struct xdag_rpc_context{
	void *data;
	int error_code;
	char * error_message;
} ;

typedef cJSON* (*xdag_rpc_function)(struct xdag_rpc_context *context, cJSON *params, cJSON* id);

struct xdag_rpc_procedure {
	char * name;
	xdag_rpc_function function;
	void *data;
};

struct xdag_rpc_connection {
	int fd;
	int pos;
	size_t buffer_size;
	char * buffer;
};

/* init xdag rpc */
extern int xdag_rpc_service_init(void);

/* stop xdag rpc */
extern int xdag_rpc_service_stop(void);

/* register procedure */
extern int xdag_rpc_service_register_procedure(xdag_rpc_function function_pointer, char *name, void *data);

/* unregister procedure */
extern int xdag_rpc_service_unregister_procedure(char *name);

#endif //XDAG_TERMINAL_H
