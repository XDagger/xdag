//
//  rpc_wrapper.c
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_wrapper.h"
#include <stdlib.h>
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
#include <pthread.h>

#include "commands.h"
#include "log.h"
#include "utils.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

#if defined(_WIN32) || defined(_WIN64)
#define poll WSAPoll
#else
#include <poll.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#define UNIX_SOCK  "unix_sock.dat"
#else
const uint32_t LOCAL_HOST_IP = 0x7f000001; // 127.0.0.1
const uint32_t APPLICATION_DOMAIN_PORT = 7676;
#endif

void rpc_processor(const char*, char*);

void rpc_processor(const char* request, char *response)
{
	strcpy(response, request);
	return;
}

const uint32_t RPC_SERVER_PORT = 7677;

#define BUFFER_SIZE 2048
static void *rpc_thread(void *arg)
{
	int rcvbufsize = BUFFER_SIZE;
	char req_buffer[BUFFER_SIZE];
	char resp_buffer[BUFFER_SIZE];
	
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		xdag_err("rpc server  : can't create socket %s\n", strerror(errno));
	}
	
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		xdag_err("rpc server   : can't set FD_CLOEXEC flag on socket %d, %s\n", sock, strerror(errno));
	}
	
	memset(&peeraddr, 0, sizeof(peeraddr));
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	peeraddr.sin_port = htons(RPC_SERVER_PORT);
	
	if (bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr))) {
		xdag_err("rpc server : socket bind failed. %s", strerror(errno));
		return 0;
	}
	
	if (listen(sock, 100) == -1) {
		xdag_err("rpc server : socket listen failed. %s", strerror(errno));
		return 0;
	}
	
	while (1) {
		int client_fd = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
		if (client_fd < 0) {
			xdag_err("rpc server : accept failed on socket %d, %s\n", sock, strerror(errno));
		}
		
		setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbufsize, sizeof(int));
		memset(req_buffer, 0, sizeof(req_buffer));
		memset(resp_buffer, 0, sizeof(resp_buffer));
		size_t len = recv(client_fd, req_buffer, sizeof(req_buffer), 0);
		if (len > BUFFER_SIZE) {
			xdag_err("rpc server : request lenght exceed!!");
			send(client_fd, "error", 6, 0);
		} else {
			rpc_processor(req_buffer, resp_buffer);
			send(client_fd, resp_buffer, strlen(resp_buffer), 0);
		}
		
		close(client_fd);
	}

	return 0;
}


int xdag_rpc_init(void)
{
	pthread_t th;
	if(pthread_create(&th, NULL, rpc_thread, NULL)) {
		return 1;
	}
	
	if(pthread_detach(th)) {
		return 1;
	}
	
	return 0;
}
