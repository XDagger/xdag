//
//  rpc_procedure.c
//  xdag
//
//  Created by Rui Xie on 6/24/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_procedure.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "../system.h"
#include "../utils/log.h"
#include "rpc_procedures.h"

/*
 *
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700	Parse error	Invalid JSON was received by the server.
 * An error occurred on the server while parsing the JSON text.
 * -32600	Invalid Request	The JSON sent is not a valid Request object.
 * -32601	Method not found	The method does not exist / is not available.
 * -32602	Invalid params	Invalid method parameter(s).
 * -32603	Internal error	Internal JSON-RPC error.
 * -32000 to -32099	Server error	Reserved for implementation-defined server-errors.
 */
#define RPC_PARSE_ERROR -32700
#define RPC_INVALID_REQUEST -32600
#define RPC_METHOD_NOT_FOUND -32601
#define RPC_INVALID_PARAMS -32603
#define RPC_INTERNAL_ERROR -32693

static struct xdag_rpc_procedure *g_procedures;
static int g_procedure_count = 0;

/* invoke procedure */
cJSON * invoke_procedure(char *name, cJSON *params, cJSON *id, char *version);
/* make error cJSON* */
cJSON * make_error(int code, char* message, cJSON * id, char *version);
/* make result cJSON* */
cJSON * make_result(cJSON * result, cJSON * id, char *version);

cJSON * make_error(int code, char* message, cJSON * id, char *version)
{
	cJSON *result_root = cJSON_CreateObject();
	cJSON *error_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(error_root, "code", code);
	cJSON_AddStringToObject(error_root, "message", message);
	cJSON_AddItemToObject(result_root, "error", error_root);
	cJSON_AddItemToObject(result_root, "id", id);
	
	if(strcmp(version, "2.0")==0) {
		cJSON_AddItemToObject(result_root, "jsonrpc", cJSON_CreateString(version));
	} else if(strcmp(version, "1.1")==0) {
		cJSON_AddItemToObject(result_root, "version", cJSON_CreateString(version));
	}
	
	return result_root;
}

cJSON * make_result(cJSON * result, cJSON * id, char *version)
{
	cJSON *result_root = cJSON_CreateObject();
	if(result) {
		cJSON_AddItemToObject(result_root, "result", result);
	}
	cJSON_AddItemToObject(result_root, "error", NULL);
	cJSON_AddItemToObject(result_root, "id", id);
	
	if(strcmp(version, "2.0")==0) {
		cJSON_AddItemToObject(result_root, "jsonrpc", cJSON_CreateString(version));
	} else if(strcmp(version, "1.1")==0) {
		cJSON_AddItemToObject(result_root, "version", cJSON_CreateString(version));
	}
	
	return result_root;
}

static void xdag_rpc_service_procedure_destroy(struct xdag_rpc_procedure *procedure)
{
	if(procedure->name){
		free(procedure->name);
		procedure->name = NULL;
	}
	
	if(procedure->data){
		free(procedure->data);
		procedure->data = NULL;
	}
}

int xdag_rpc_service_register_procedure(xdag_rpc_function function_pointer, char *name, void * data)
{
	int i = g_procedure_count++;
	if(!g_procedures) {
		g_procedures = malloc(sizeof(struct xdag_rpc_procedure));
	} else {
		struct xdag_rpc_procedure * ptr = realloc(g_procedures, sizeof(struct xdag_rpc_procedure) * g_procedure_count);
		if(!ptr) {
			xdag_err("rpc server : realloc failed!");
			return -1;
		}
		g_procedures = ptr;
	}
	
	if((g_procedures[i].name = strdup(name)) == NULL) {
		return -1;
	}
	
	g_procedures[i].function = function_pointer;
	g_procedures[i].data = data;
	return 0;
}

int xdag_rpc_service_unregister_procedure(char *name)
{
	int i, found = 0;
	if(g_procedures){
		for (i = 0; i < g_procedure_count; i++){
			if(found) {
				g_procedures[i-1] = g_procedures[i];
			} else if(!strcmp(name, g_procedures[i].name)){
				found = 1;
				xdag_rpc_service_procedure_destroy(&(g_procedures[i]));
			}
		}
		if(found){
			g_procedure_count--;
			if(g_procedure_count){
				struct xdag_rpc_procedure * ptr = realloc(g_procedures, sizeof(struct xdag_rpc_procedure) * g_procedure_count);
				if(!ptr){
					xdag_err("rpc server : realloc failed!");
					return -1;
				}
				g_procedures = ptr;
			}else{
				g_procedures = NULL;
			}
		} else {
			xdag_err("rpc server : procedure '%s' not found\n", name);
		}
	} else {
		xdag_err("rpc server : procedure '%s' not found\n", name);
		return -1;
	}
	return 0;
}

cJSON * invoke_procedure(char *name, cJSON *params, cJSON *id, char *version)
{
	cJSON *returned = NULL;
	int procedure_found = 0;
	struct xdag_rpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = g_procedure_count;
	while (i--) {
		if(!strcmp(g_procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = g_procedures[i].data;
			returned = g_procedures[i].function(&ctx, params, id, version);
			break;
		}
	}
	
	if(!procedure_found) {
		return make_error(RPC_METHOD_NOT_FOUND, strdup("Method not found."), id, version);
	} else {
		if(ctx.error_code) {
			if(returned) {
				cJSON_Delete(returned);
			}
			return make_error(ctx.error_code, ctx.error_message, id, version);
		} else {
			return make_result(returned, id, version);
		}
	}
}

/* handle rpc request */
cJSON * xdag_rpc_handle_request(char* buffer)
{
	cJSON *root = NULL, *result = NULL;
	const char *end_ptr = NULL;
	
	if((root = cJSON_ParseWithOpts(buffer, &end_ptr, 0)) != NULL) {
		
		char * str_result = cJSON_Print(root);
		xdag_debug("Valid JSON Received:\n%s\n", str_result);
		free(str_result);
		
		if(root->type == cJSON_Object) {
			cJSON *method, *params, *id, *verjson;
			char version[8] = "1.0";
			method = cJSON_GetObjectItem(root, "method");
			
			verjson = cJSON_GetObjectItem(root, "jsonrpc"); /* rpc 2.0 */
			if(!verjson) {
				verjson = cJSON_GetObjectItem(root, "version"); /* rpc 1.1 */
			}
			
			if(verjson) {
				strcpy(version, verjson->valuestring);
			}
			
			if(method != NULL && method->type == cJSON_String) {
				params = cJSON_GetObjectItem(root, "params");
				if(params == NULL|| params->type == cJSON_Array || params->type == cJSON_Object) {
					id = cJSON_GetObjectItem(root, "id");
					if(id == NULL|| id->type == cJSON_String || id->type == cJSON_Number) {
						//We have to copy ID because using it on the reply and deleting the response Object will also delete ID
						cJSON * id_copy = NULL;
						if(id != NULL) {
							id_copy = (id->type == cJSON_String) ? cJSON_CreateString(id->valuestring):cJSON_CreateNumber(id->valueint);
						}
						xdag_debug("Method Invoked: %s\n", method->valuestring);
						
						result = invoke_procedure(method->valuestring, params, id_copy, version);
					}
				}
			}
		}
		
		if(!result) {
			result = make_error(RPC_PARSE_ERROR, strdup("Request parse error."), 0, "2.0");
		}
	} else {
		result = make_error( RPC_PARSE_ERROR, strdup("Request parse error."), 0, "2.0");
	}
	
	if(root) {
		cJSON_Delete(root);
	}
	
	return result;
}

