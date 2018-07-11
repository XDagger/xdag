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

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../utils/log.h"
#include "../system.h"
#include "rpc_procedure.h"
#include "../uthash/utlist.h"

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
#define RPC_WHITE_ADDR_LEN          64
#define RPC_WHITE_MAX               8


const uint32_t RPC_SERVER_PORT = 7677; //default http json-rpc port 7677

typedef struct rpc_white_element {
    struct rpc_white_element *prev, *next;
    struct in_addr addr;
}rpc_white_element;

struct rpc_white_element *g_rpc_white_host = NULL;

static int rpc_host_addr_check_ipv4(const char *host)
{

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
    for(i = 0; i < 3; ++i){
      if (c[i] != '.'){ 
        return 0;
      }
    }
    
    for(i = 0; i < 4; ++i){
      if (n[i] > 255 || n[i] < 0){
        return 0;
      }
    }
    return 1;
  }else{
    return 0;
  }
}

static int rpc_white_host_check(struct sockaddr_in peeraddr)
{

  rpc_white_element *element = NULL ;
    
  if (!g_rpc_white_host){
        return 1;
  }

  LL_FOREACH(g_rpc_white_host,element)
  {
    if (element->addr.s_addr == peeraddr.sin_addr.s_addr){
        return 1;
    }
  }

  return 0;
}


static int rpc_white_host_add(const char *host)
{
  rpc_white_element *new_white_host = NULL,*tmp = NULL;
  int white_num = 0;
  struct in_addr addr = {0};

  if (!rpc_host_addr_check_ipv4(host)){
    xdag_err("ip address is invalid");
    return -1;
  }

  LL_COUNT(g_rpc_white_host,new_white_host,white_num);
  if (white_num >= RPC_WHITE_MAX){
    xdag_err("white list number is up to maximum");
    return -2;
  }

  addr.s_addr = inet_addr(host);
  LL_FOREACH_SAFE(g_rpc_white_host,new_white_host,tmp)
  {
    if (new_white_host->addr.s_addr == addr.s_addr){
        xdag_err("host [%s] is in the rpc white list.",host);
        return -3;
    }
  }

  new_white_host = malloc(sizeof(rpc_white_element));
  if (NULL == new_white_host){
    xdag_err("memory is not enough.");
    return -4;
  }

  new_white_host->addr.s_addr = addr.s_addr;
  DL_APPEND(g_rpc_white_host, new_white_host);

  return 0;
}

static int rpc_white_host_del(const char *host)
{

  rpc_white_element *node = NULL ,*tmp = NULL,  *del_node = NULL;
  struct in_addr addr = {0};

  LL_FOREACH_SAFE(g_rpc_white_host,node,tmp)
  {
    addr.s_addr = inet_addr(host);
    if (addr.s_addr == node->addr.s_addr){
        del_node = node;
        LL_DELETE(g_rpc_white_host, del_node);
        free(del_node);
        return 0;
    }
  }

  return -1;
}

static char  *rpc_white_host_query()
{

    static char result[RPC_WHITE_MAX * RPC_WHITE_ADDR_LEN] = {0};
    rpc_white_element *element = NULL, *tmp = NULL;
    char new_host[RPC_WHITE_ADDR_LEN] = {0};

    LL_FOREACH_SAFE(g_rpc_white_host,element,tmp)
    {
        memset(new_host, 0, sizeof(new_host));
        
        sprintf(new_host, "[%s]\n",inet_ntoa(element->addr));
        strcat(result, new_host);
    }
    
    return result;
}


int rpc_white_command(void *out, char *type, const char *address)
{

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
        fprintf(out, "add address[%s] failed:only allowed 8 white address \n", address);
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


static int send_response(struct xdag_rpc_connection * conn,const char *response) {
	int fd = conn->fd;
	xdag_debug("JSON Response:\n%s\n", response);
	write(fd, response, strlen(response));
	write(fd, "\n", 1);
	return 0;
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

/* handle rpc request thread */
static void* rpc_handle_thread(void *arg)
{
	struct xdag_rpc_connection* conn = (struct xdag_rpc_connection *)arg;
	
	cJSON * result = xdag_rpc_handle_request(conn->buffer);
	char *response = cJSON_Print(result);
	send_response(conn, response);
	free(response);
	
	if(result) {
		cJSON_Delete(result);
	}
	close_connection(conn);
	
	return 0;
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
		xdag_err("rpc service : socket bind failed. error : %s", strerror(errno));
		return 0;
	}
	
	if(listen(sock, 100) == -1) {
		xdag_err("rpc service : socket listen failed. error : %s", strerror(errno));
		return 0;
	}
	
	while (1) {
		int client_fd = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
		if(client_fd < 0) {
			xdag_err("rpc service : accept failed on socket %d, error : %s\n", sock, strerror(errno));
			continue;
		}
        
		memset(req_buffer, 0, sizeof(req_buffer));
		size_t len = read(client_fd, req_buffer, BUFFER_SIZE);
		
		struct xdag_rpc_connection * conn = create_connection(client_fd, req_buffer, len);

        if (!rpc_white_host_check(peeraddr)){
            xdag_warn("rpc client is not in white list : %s,close",inet_ntoa(peeraddr.sin_addr));
            send_response(conn, "connection refused by white host");
            sleep(10);
            close_connection(conn);
            continue;
        }
        
		pthread_t th;
		int err = pthread_create(&th, 0, rpc_handle_thread, conn);
		if(err) {
			xdag_err("rpc service : create thread failed. error : %s", strerror(err));
			close_connection(conn);
			continue;
		}
		
		err = pthread_detach(th);
		if(err) {
			xdag_err("rpc service : detach thread failed. error : %s", strerror(err));
		}
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
