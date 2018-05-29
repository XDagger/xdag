//
//  rpc_service.c
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_service.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#endif

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../utils/log.h"
#include "../system.h"
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

const uint32_t RPC_SERVER_PORT = 7677; //default http json-rpc port 7677

static struct xdag_rpc_procedure *g_procedures;
static int g_procedure_count = 0;

static int send_response(struct xdag_rpc_connection * conn, char *response) {
	int fd = conn->fd;
	xdag_debug("JSON Response:\n%s\n", response);
	write(fd, response, strlen(response));
	write(fd, "\n", 1);
	return 0;
}

static int send_error(struct xdag_rpc_connection * conn, int code, char* message, cJSON * id, char *version) {
	int return_value = 0;
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
	
	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct xdag_rpc_connection * conn, cJSON * result, cJSON * id, char *version) {
	int return_value = 0;
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
	
	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	return return_value;
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

int xdag_rpc_service_register_procedure(xdag_rpc_function function_pointer, char *name, void * data) {
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

int xdag_rpc_service_unregister_procedure(char *name) {
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

static int invoke_procedure(struct xdag_rpc_connection * conn, char *name, cJSON *params, cJSON *id, char *version) {
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
		return send_error(conn, RPC_METHOD_NOT_FOUND, strdup("Method not found."), id, version);
	} else {
		if(ctx.error_code) {
			return send_error(conn, ctx.error_code, ctx.error_message, id, version);
		} else {
			return send_result(conn, returned, id, version);
		}
	}
}

/* create xdag connection */
static struct xdag_rpc_connection* create_connection(int fd, const char* req_buffer, size_t len)
{
	const char *body = strstr(req_buffer, "\r\n\r\n");
	if(body) {
		body += 4;
	} else {
		body = req_buffer;
	}
	
	char *req = (char*)malloc(strlen(body));
	memcpy(req, body, strlen(body));
	
	struct xdag_rpc_connection *conn = (struct xdag_rpc_connection*)malloc(sizeof(struct xdag_rpc_connection));
	conn->buffer = req;
	conn->buffer_size = len;
	conn->fd = fd;
	return conn;
}

/* close xdag connection */
static void close_connection(struct xdag_rpc_connection* conn)
{
	close(conn->fd);
	free(conn->buffer);
	free(conn);
}

/* handle connection */
static int rpc_handle_connection(struct xdag_rpc_connection* conn)
{
	int ret = 0;
	cJSON *root;
	const char *end_ptr = NULL;
	
	if((root = cJSON_ParseWithOpts(conn->buffer, &end_ptr, 0)) != NULL) {
		
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
						ret = invoke_procedure(conn, method->valuestring, params, id_copy, version);
						close_connection(conn);
						return ret;
					}
				}
			}
		}
		cJSON_Delete(root);
		ret = send_error(conn, RPC_PARSE_ERROR, strdup("Request parse error."), 0, "2.0"); //use rpc 2.0 as default version
		close_connection(conn);
		return ret;
	} else {
		ret = send_error(conn, RPC_PARSE_ERROR, strdup("Request parse error."), 0, "2.0"); //use rpc 2.0 as default version
		close_connection(conn);
		return ret;
	}
}

#define BUFFER_SIZE 2048

/* rpc service thread */
static void *rpc_service_thread(void *arg)
{
	int rpc_port = *(int*)arg;
	char req_buffer[BUFFER_SIZE];
	
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock == INVALID_SOCKET) {
		xdag_err("rpc service : can't create socket %s\n", strerror(errno));
	}
	
	if(fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		xdag_err("rpc service : can't set FD_CLOEXEC flag on socket %d, %s\n", sock, strerror(errno));
	}
	
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;
//	peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	peeraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	peeraddr.sin_port = htons(rpc_port);
	
	if(bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr))) {
		xdag_err("rpc service : socket bind failed. %s", strerror(errno));
		return 0;
	}
	
	if(listen(sock, 100) == -1) {
		xdag_err("rpc service : socket listen failed. %s", strerror(errno));
		return 0;
	}
	
	while (1) {
		int client_fd = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
		if(client_fd < 0) {
			xdag_err("rpc service : accept failed on socket %d, %s\n", sock, strerror(errno));
			continue;
		}
		
		memset(req_buffer, 0, sizeof(req_buffer));
		size_t len = read(client_fd, req_buffer, BUFFER_SIZE);

		rpc_handle_connection(create_connection(client_fd, req_buffer, len));
	}

	return 0;
}

/* init xdag rpc service */
int xdag_rpc_service_init(int port)
{
	static int rpc_port;
	rpc_port = port;
	if(!rpc_port) {
		rpc_port = RPC_SERVER_PORT;
	}
	
	pthread_t th;
	int err = pthread_create(&th, NULL, rpc_service_thread, (void*)&rpc_port);
	if(err != 0) {
		printf("create rpc_service_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	
	err = pthread_detach(th);
	if(err != 0) {
		printf("detach rpc_service_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	
	/* init rpc procedures */
	xdag_rpc_init_procedures();
	
	return 0;
}

/* stop xdag rpc service */
int xdag_rpc_service_stop(void)
{
	return 0;
}
