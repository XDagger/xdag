//
//  rpc_procedure.h
//  xdag
//
//  Created by Rui Xie on 6/24/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef XDAG_RPC_PROCEDURE_H
#define XDAG_RPC_PROCEDURE_H

#include "cJSON.h"
#include "cJSON_Utils.h"
#include "rpc_procedures.h"

struct xdag_rpc_context{
	void *data;
	int error_code;
	char * error_message;
	char rpc_version[8];
} ;

typedef cJSON* (*xdag_rpc_function)(struct xdag_rpc_context *context, cJSON *params, cJSON *id, char *version);

struct xdag_rpc_procedure {
	char * name;
	xdag_rpc_function function;
	void *data;
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* register procedure */
extern int xdag_rpc_service_register_procedure(xdag_rpc_function function_pointer, char *name, void *data);

/* unregister procedure */
extern int xdag_rpc_service_unregister_procedure(char *name);

/* handle rpc request */
extern cJSON *xdag_rpc_handle_request(char* buffer);

#ifdef __cplusplus
};
#endif
		
#endif //XDAG_RPC_PROCEDURE_H
