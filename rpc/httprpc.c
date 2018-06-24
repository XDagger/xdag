//
//  httprpc.c
//  xdag
//
//  Created by Peter Liu on 16/06/20.
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
#include <string.h>
#endif
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <event2/http_struct.h>

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>

#include "base64.h"

#include "lst.h"
#include "log.h"
#include "../json-rpc/rpc_service.h"
#include "httprpc.h"


/***********************************************
*
*                    defines
*
************************************************/
#define HTTP_RPC_VARIBLES_LEN            256
#define HTTP_RPC_ADDR_LEN                64
#define HTTP_RPC_WHITE_MAX               8
#define HTTP_RPC_AUTH_LEN                (HTTP_RPC_VARIBLES_LEN*4)

/***********************************************
*
*                    structs and global variables
*
************************************************/

//! HTTP status codes
enum HTTP_RPC_STATUS_CODE
{
    HTTPRPC_OK                    			  = 200,
    HTTPRPC_BAD_REQUEST                   = 400,
    HTTPRPC_UNAUTHORIZED                  = 401,
    HTTPRPC_FORBIDDEN                     = 403,
    HTTPRPC_NOT_FOUND                     = 404,
    HTTPRPC_BAD_METHOD                    = 405,
    HTTPRPC_INTERNAL_SERVER_ERROR         = 500,
    HTTPRPC_SERVICE_UNAVAILABLE           = 503,
};

char g_httprpc_user[HTTP_RPC_VARIBLES_LEN];
char g_httprpc_password[HTTP_RPC_VARIBLES_LEN];

char g_httprpc_addr[HTTP_RPC_ADDR_LEN] = {"0.0.0.0"};
int g_httprpc_port = 7678;

LIST g_httproc_white_host;

typedef struct {
  NODE node;
  char addr[HTTP_RPC_ADDR_LEN];
}rpc_white_host;

/***********************************************
*
*                    functions
*
************************************************/

static int rpc_host_addr_check_ipv4(const char *host){

  int n[4];
  char c[4];

  if (!host){
    return 0;
  }

  if (sscanf(host, "%d%c%d%c%d%c%d%c",
             &n[0], &c[0], &n[1], &c[1],
             &n[2], &c[2], &n[3], &c[3]) == 7)

  {
    int i;
    for(i = 0; i < 3; ++i)
      if (c[i] != '.')
        return 0;
    for(i = 0; i < 4; ++i)
      if (n[i] > 255 || n[i] < 0)
        return 0;
    return 1;
  } else
    return 0;
}

static int rpc_white_host_add(const char *host){
  rpc_white_host *new_white_host = NULL;

  if (!rpc_host_addr_check_ipv4(host)){
    xdag_err("ip address is invalid");
    return -1;
  }

  if (lstCount(&g_httproc_white_host) >= HTTP_RPC_WHITE_MAX){
    xdag_err("white list number is up to maximum");
    return -2;
  }

  new_white_host = malloc(sizeof(rpc_white_host));
  if (NULL == new_white_host){
    xdag_err("memories is not enough.");
    return -3;
  }

  strcpy(new_white_host->addr, host);

  lstAdd(&g_httproc_white_host, (NODE *)new_white_host);

  return 0;
}

static int rpc_white_host_del(const char *host){

  NODE *node = NULL;
  rpc_white_host *del_node = NULL;

  node = lstFirst(&g_httproc_white_host);
  while (NULL != node){
    if (0 == strcmp(host ,((rpc_white_host *)node)->addr)){
        del_node = (rpc_white_host *)node;
        break;
    }

    node = lstNext(node);
  }

  if (NULL != del_node){
    lstDelete(&g_httproc_white_host, (NODE *)del_node);
    return 0;
  }else{
    return -1;
  }
}

static char  *rpc_white_host_query(){

    static int white_host_num = 0;
    static char result[HTTP_RPC_WHITE_MAX * HTTP_RPC_ADDR_LEN] = {0};
    int new_numer = 0;
    NODE *node = NULL;
    rpc_white_host *host = NULL;

    new_numer = lstCount(&g_httproc_white_host);
    if (!white_host_num || new_numer != white_host_num){
      node = lstFirst(&g_httproc_white_host);
      while (NULL != node){

        char new_host[HTTP_RPC_ADDR_LEN] = {0};

        host = (rpc_white_host *)node;
        sprintf(new_host,"[%s]",host->addr);
        strcat(result, new_host);
        node = lstNext(node);
      }
    }

    return result;
}

/* password as key, username do hmac-sha256,then convert base64
 * return 1 means passed, 0 means not authorized
*/
static int http_rpc_authorized(const char *auth_string){

    unsigned char result[HTTP_RPC_AUTH_LEN] = {0};
    unsigned int result_len = HTTP_RPC_AUTH_LEN,key_len = strlen(g_httprpc_password);
    HMAC_CTX ctx;

    unsigned char auth_encode_string[HTTP_RPC_AUTH_LEN] = {0};

  	if ( 0 == strlen(g_httprpc_user)  || 0 == key_len){
  		return 1;
  	}

    if (NULL == auth_string){
      xdag_err("auth_string is NULL\n");
      return 0;
    }

    HMAC_CTX_init(&ctx);

    HMAC_Init_ex(&ctx, g_httprpc_password, key_len, EVP_sha256(), NULL);
    HMAC_Update(&ctx, (unsigned char*)&g_httprpc_user, strlen(g_httprpc_user));
    HMAC_Final(&ctx, result, &result_len);
    HMAC_CTX_cleanup(&ctx);

    encode_base64(auth_encode_string, result, result_len);
    if (0 == strcmp((const char *)auth_encode_string, (const char *)auth_string)){
        return 1;
    }else{
        return 0;
    }
}

static char *http_rpc_read_body(struct evhttp_request *req){

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

static const char *request_get_head(struct evhttp_request *req, const char *key)
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

static void  http_rpc_request_handle(struct evhttp_request *req, void *arg)
{
	int auth = 0, iswhitehost = 0,ret = 0;
	char *pbody = NULL,*result = NULL;
  const char *pauth = NULL;
  NODE *ptr = NULL;
  struct evbuffer *response;

  /* ip white list */
  if (lstCount(&g_httproc_white_host) > 0){

    ptr = lstFirst(&g_httproc_white_host);
    while ( NULL != ptr){
      if (0 == strcmp(req->remote_host ,((rpc_white_host *)ptr)->addr)){
        iswhitehost = 1;
        break;
      }
      ptr = lstNext(ptr);
    }
  }

  if (0 == iswhitehost){
    xdag_crit("[%s]receive request from:%s, not allowed\n",__FUNCTION__, req->remote_host);
    evhttp_send_reply(req, HTTPRPC_UNAUTHORIZED, "not allow to request", NULL);
    return;
  }

	if (EVHTTP_REQ_POST != evhttp_request_get_command(req)){
		evhttp_send_reply(req, HTTPRPC_BAD_REQUEST,
								"JSONRPC server handles only POST requests", NULL);
		return;
	}

  pauth = request_get_head(req, "Authorization");
  auth = http_rpc_authorized(pauth);
  if (!auth){
	  evhttp_send_reply(req, HTTPRPC_UNAUTHORIZED,"invalid authorization",NULL);
	return;
	}

  pbody = http_rpc_read_body(req);
	if (!pbody){
		evhttp_send_reply(req, HTTPRPC_BAD_METHOD, "invalid body",NULL);
		return;
	}

  ret = xdag_rpc_command_procedure(pbody, &result);
  if (0 != ret){
    evhttp_send_reply(req,HTTPRPC_BAD_REQUEST,"request need json string",NULL);
    if (result){
       free (result);
    }
    return;
  }

  if ( NULL == result ){
    evhttp_send_reply(req,HTTPRPC_INTERNAL_SERVER_ERROR,"system error",NULL);
    return;
  }

  response = evbuffer_new();
  if ( NULL == response ){
    evhttp_send_reply(req,HTTPRPC_INTERNAL_SERVER_ERROR,"system error",NULL);
    return;
  }

  if (evbuffer_add(response, result, strlen(result))){
    xdag_err("evbuffer_add add result failed.:%s",result);
    return;
  }

  evhttp_send_reply(req, HTTPRPC_OK , "SUCCESS", response);
  free(result);
  evbuffer_free(response);
  return;
}


static void *http_rpc_server_thread(){

  struct event_base* base = event_base_new();
  if (!base){
    xdag_err("create event_base failed!\n");
    return NULL;
  }

  struct evhttp* http = evhttp_new(base);
  if (!http)
  {
      xdag_err("create evhttp failed!\n");
      return NULL;
  }


  if (evhttp_bind_socket(http, g_httprpc_addr, g_httprpc_port) != 0)
  {
      xdag_err("bind socket failed! port:%d\n",g_httprpc_port);
      return NULL;
  }

  printf("start http rpc, listen port :%d....\n",g_httprpc_port);
  evhttp_set_gencb(http, http_rpc_request_handle, NULL);

  event_base_dispatch(base);

  return NULL;
}

int http_rpc_start(const char *username ,const char *passwd, int port){

  int len = 0;
  if ( NULL != passwd && NULL != username){

    len = strlen(passwd);
    if (len > sizeof(g_httprpc_password) - 1){
      xdag_err("password name is too long");
      return -1;
    }

    len = strlen(username);
    if (len > sizeof(g_httprpc_user) - 1){
      xdag_err("user name is too long.");
      return -1;
    }

    strcpy(g_httprpc_password, passwd);
    strcpy(g_httprpc_user, username);
  }

  lstInit(&g_httproc_white_host);

	if (0 == strlen(g_httprpc_addr) ){
		memset(g_httprpc_addr, 0, sizeof(g_httprpc_addr));
		strcpy(g_httprpc_addr,"0.0.0.0");
	}

  g_httprpc_port = port?port:7678;

  pthread_t th;
  int err = pthread_create(&th, NULL, http_rpc_server_thread, NULL);
  if(err != 0) {
    xdag_err("create http_service_thread failed, error : %s\n", strerror(err));
    return -1;
  }

  err = pthread_detach(th);
  if(err != 0) {
    xdag_err("detach http_service_thread failed, error : %s\n", strerror(err));
    return -1;
  }

  return 0;
}

int http_rpc_command(void *out, char *type, const char *address){

  int result = 0;
  char *list = NULL;

  if (!strcmp("-a", type)){
    result = rpc_white_host_add (address);
    switch (result){
      case 0:
        fprintf(out, "add address[%s] success \n", address);
        break;
      case -1:
        fprintf(out, "add address[%s] failed:address is invalid \n", address);
        break;
      case -2:
        fprintf(out, "add address[%s] failed:only allow 8 address \n", address);
        break;
      default:
        fprintf(out, "add address[%s] failed:system error ,tray again later\n", address);
    }
  }else if (!strcmp("-d", type)){
    result = rpc_white_host_del (address);
    if (0 == result){
      fprintf(out, "delete address[%s] success \n", address);
    }else{
      fprintf(out, "delete address[%s] failed \n", address);
    }
  }else if (!strcmp("-l", type)){
    list = rpc_white_host_query();
    fprintf(out, "white address are:%s\n", list );
  }else{
    return -1;
  }
  return 0;
}
