//
//  httprpc.c
//  xdag
//
//  Created by Peter Liu on 16/06/18.
//  Copyright  2018 xrdavies. All rights reserved.
//

/*********************************************** 
*
*                    include files
*
************************************************/
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#endif
#include <sys/stat.h>  

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
  
#include <event2/event.h>  
#include <event2/http.h>  
#include <event2/buffer.h>  
#include <event2/util.h>  
#include <event2/keyvalq_struct.h>  

#include "log.h"


/*********************************************** 
*
*                    defines 
*
************************************************/
#define HTTP_RPC_VARIBLES_LEN          256
#define HTTP_RPC_WHITE_LIST              256
#define HTTP_RPC_ADDR_LEN                64

/*********************************************** 
*
*                    structs and global variables
*
************************************************/

//! HTTP status codes
enum HTTP_RPC_STATUS_CODE
{
    HTTPRPC_OK                    			  = 200,
    HTTPRPC_BAD_REQUEST                  = 400,
    HTTPRPC_UNAUTHORIZED                 = 401,
    HTTPRPC_FORBIDDEN                       = 403,
    HTTPRPC_NOT_FOUND                      = 404,
    HTTPRPC_BAD_METHOD                    = 405,
    HTTPRPC_INTERNAL_SERVER_ERROR = 500,
    HTTPRPC_SERVICE_UNAVAILABLE      = 503,
};

char g_httprpc_user[HTTP_RPC_VARIBLES_LEN];
char g_httprpc_password[HTTP_RPC_VARIBLES_LEN];

char g_httprpc_addr[HTTP_RPC_ADDR_LEN] = {"0.0.0.0"};
int g_httprpc_port = 7678;

/*********************************************** 
*
*                    functions 
*
************************************************/

static int http_rpc_authorized(const char *auth_string){

	if ( 0 == strlen(g_httprpc_user)  || 0 == strlen(g_httprpc_password){
		return 1
	}

	

	 return 1;
}

inline char *http_rpc_read_body(struct evhttp_request *req){

	char *rv = NULL;
	const char* data = NULL;
	size_t size = 0;
	struct evbuffer* buf = NULL;

	buf = evhttp_request_get_input_buffer(req);
	if (!buf){
		return NULL;
	}
		   
	size = evbuffer_get_length(buf);

	/** Trivial implementation: if this is ever a performance bottleneck,
	* internal copying can be avoided in multi-segment buffers by using
	* evbuffer_peek and an awkward loop. Though in that case, it'd be even
	* better to not copy into an intermediate string but use a stream
	* abstraction to consume the evbuffer on the fly in the parsing algorithm.
	*/
		
	data = (const char*)evbuffer_pullup(buf, size);
	if (!data) {
		// returns NULL in case of empty buffer
		xdag_err("empty buffer read.");
		return NULL;
	}

	rv = malloc(size+1);
	if (!rv){
		xdag_err("not enough memories.");
		return NULL;
	}

	memcpy(rv, data, size);
	rv[size] = 0;
	
	return rv;

}

const char *request_get_head(struct evhttp_request *req, const char *key)  
{  
	struct evkeyvalq *headers;  
	const char* val = NULL;
	 
	headers = evhttp_request_get_input_headers(req);  
    	if (NULL == headers){
		return NULL;
	}
		
   	val = evhttp_find_header(headers, key);
	return val;
}  


static void   http_rpc_request_handle(struct evhttp_request *req, void *arg)  
{  
	int auth = 0;
	char *pbody = NULL,pauth = NULL;

	if (EVHTTP_REQ_POST != evhttp_request_get_command(req)){
		evhttp_send_reply(req, HTTPRPC_BAD_REQUEST, 
								"JSONRPC server handles only POST requests", NULL);  
		return;
	}

    pauth = request_get_head(req, "authorization");
    auth = http_rpc_authorized(pauth);
    if (!auth){

		evhttp_send_reply(req,HTTPRPC_UNAUTHORIZED,"invalid authorization",NULL);
		return;
	}

  	pbody = http_rpc_read_body(req);  
	if (!pbody){
		evhttp_send_reply(req,HTTPRPC_BAD_METHOD,"invalid body",NULL);
		return;
	}
}  


int http_rpc_start(){

	xdag_debug("start http rpc....");

	if (0 == strlen(g_httprpc_addr) ){
		memset(g_httprpc_addr, 0, sizeof(g_httprpc_addr));
		strcpy(g_httprpc_addr,"0.0.0.0");
	}

	struct event_base* base = event_base_new();
    	if (!base)
    	{
        xdag_err("create event_base failed!\n");
        return -1;
    }

    struct evhttp* http = evhttp_new(base);
    if (!http)
    {
        xdag_err("create evhttp failed!\n");
        return -2;
    }

    if (evhttp_bind_socket(http, g_httprpc_addr, g_httprpc_port) != 0)
    {
        xdag_err("bind socket failed! port:%d\n", g_httprpc_port);
        return -3;
    }

    evhttp_set_gencb(http, http_rpc_request_handle, NULL);

    event_base_dispatch(base);

	return 0;

}

