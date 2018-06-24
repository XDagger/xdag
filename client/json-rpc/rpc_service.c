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
#include "rpc_procedure.h"

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

static int send_response(struct xdag_rpc_connection * conn, char *response) {
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
