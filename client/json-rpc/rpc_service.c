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
#include <arpa/inet.h>
#if defined(_WIN32) || defined(_WIN64)
#define poll WSAPoll
#else
#include <poll.h>
#endif
#include <pthread.h>

#include "../utils/log.h"
#include "../utils/utils.h"
#include "../system.h"
#include "rpc_procedure.h"
#include "rpc_commands.h"

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

#define BUFFER_SIZE 2048
#define MAX_OPEN 128
#define DEFAULT_RPC_PORT 7667 //default http json-rpc port 7667
int g_rpc_stop = 1; // 0 running, 1 stopped, 2 stopping in progress
int g_rpc_port = DEFAULT_RPC_PORT;
int g_rpc_xfer_enable = 0; // 0 disable xfer, 1 enable xfer
int g_rpc_white_enable = 1; // 0 disable white list, 1 enable white list

static struct pollfd *g_fds = NULL;
struct xdag_rpc_connection {
	struct pollfd fd;
	int is_http;
	int pos;
};

static void wrap_http(int fd)
{
	write(fd, "HTTP/1.1 200 OK\r\n", 17);
	write(fd, "Content-Type: application/json\r\n\r\n", 34);
}

static int send_response(struct xdag_rpc_connection * conn,const char *response) {
	int fd = conn->fd.fd;
	if(conn->is_http) {
		wrap_http(fd);
	}
	write(fd, response, strlen(response));
	return 0;
}

/* create xdag connection */
static struct xdag_rpc_connection* create_connection(struct pollfd *fd)
{
	struct xdag_rpc_connection *conn = (struct xdag_rpc_connection*)malloc(sizeof(struct xdag_rpc_connection));
	memset(conn, 0, sizeof(struct xdag_rpc_connection));
	conn->fd = *fd;
	return conn;
}

/* close xdag connection */
static void close_connection(struct xdag_rpc_connection* conn)
{
	shutdown(conn->fd.fd, SHUT_WR);
	recv(conn->fd.fd, NULL, 0, 0);
	close(conn->fd.fd);
	free(conn);
}

/* handle rpc request thread */
static void* rpc_handle_thread(void *arg)
{
	struct xdag_rpc_connection* conn = (struct xdag_rpc_connection *)arg;

	char req_buffer[BUFFER_SIZE] = {0};
	memset(req_buffer, 0, sizeof(req_buffer));
	size_t len = read(conn->fd.fd, req_buffer, BUFFER_SIZE);
	if(len <= 0) {
		close_connection(conn);
		return 0;
	}

	char *body = strstr(req_buffer, "\r\n\r\n");
	if(body) {
		conn->is_http = 1;
		body += 4;
	} else {
		body = req_buffer;
	}
	
	cJSON * result = xdag_rpc_handle_request(body);
	char *response = cJSON_PrintUnformatted(result);
	send_response(conn, response);
	free(response);
	
	if(result) {
		cJSON_Delete(result);
	}
	close_connection(conn);
	
	return 0;
}

/* rpc service thread */
static void *rpc_service_thread(void *arg)
{
	int rpc_port = *(int*)arg;
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	memset(&peeraddr, 0, sizeof(struct sockaddr_in));
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	peeraddr.sin_port = htons(rpc_port);
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock == INVALID_SOCKET) {
		xdag_err("rpc service : can't create socket %s", strerror(errno));
	}

	int reuse_opt = 1;
	struct linger linger_opt = {1, 0};
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(reuse_opt)) < 0) {
		xdag_err("rpc service : can't set SO_REUSEADDR flag on socket %d, error : %s", sock, strerror(errno));
		return 0;
	}

	if(setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt)) < 0) {
		xdag_err("rpc service : can't set SO_LINGER flag on socket %d, error : %s", sock, strerror(errno));
		return 0;
	}

	if(bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr))) {
		xdag_err("rpc service : socket bind failed. error : %s", strerror(errno));
		return 0;
	}
	
	if(listen(sock, 100) == -1) {
		xdag_err("rpc service : socket listen failed. error : %s", strerror(errno));
		return 0;
	}

	g_fds = malloc((MAX_OPEN + 1) * sizeof(struct pollfd));
	memset(g_fds, 0, (MAX_OPEN + 1) * sizeof(struct pollfd));
	g_fds[0].fd = sock;
	g_fds[0].events = POLLIN | POLLERR;
	int i = 0;
	for(i = 1; i < MAX_OPEN; ++i) {
		g_fds[i].fd = -1;
	}

	xdag_mess("RPC service startd.");

	int ready = 0;
	while (0 == g_rpc_stop) {
		int res = poll(g_fds, (MAX_OPEN + 1), 1000);
		if(!res) {
			continue;
		}

		if(g_fds[0].revents & POLLIN) {
			g_fds[0].revents |= ~POLLIN;
			ready = 1;
			int client_fd = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
			if(client_fd < 0) {
				xdag_err("rpc service : accept failed on socket %d, error : %s", sock, strerror(errno));
				continue;
			}

			if(!xdag_rpc_command_host_check(peeraddr)) {
				xdag_warn("rpc client is not in white list : %s, closed", inet_ntoa(peeraddr.sin_addr));
				struct xdag_rpc_connection *conn = (struct xdag_rpc_connection*)malloc(sizeof(struct xdag_rpc_connection));
				memset(conn, 0, sizeof(struct xdag_rpc_connection));
				g_fds[MAX_OPEN].fd = client_fd;
				conn->fd = *(g_fds + MAX_OPEN);
				send_response(conn, "connection refused by white host");
				close_connection(conn);
			} else {
				for(i = 1; i < MAX_OPEN; ++i) {
					if(g_fds[i].fd == -1) {
						g_fds[i].fd = client_fd;
						g_fds[i].events = POLLIN;
						g_fds[i].revents = 0;
						break;
					}
				}

				if(i == MAX_OPEN) {
					xdag_warn("rpc service : too many connections, max connections : %d, close connection from %s", MAX_OPEN, inet_ntoa(peeraddr.sin_addr));
					struct xdag_rpc_connection *conn = (struct xdag_rpc_connection*)malloc(sizeof(struct xdag_rpc_connection));
					memset(conn, 0, sizeof(struct xdag_rpc_connection));
					g_fds[MAX_OPEN].fd = client_fd;
					conn->fd = *(g_fds + MAX_OPEN);
					send_response(conn, "too many connections, please try later.");
					close_connection(conn);
				}
			}
		}

		for(i = 1; i < MAX_OPEN; ++i) {
			struct pollfd *p = g_fds + i;
			if(p->fd == -1) {
				continue;
			}

			if(p->revents & POLLNVAL) {
				close(p->fd);
				p->fd = -1;
				continue;
			}

			if(p->revents & POLLERR) {
				close(p->fd);
				p->fd = -1;
				continue;
			}

			if(p->revents & POLLHUP) {
				close(p->fd);
				p->fd = -1;
				continue;
			}

			if(p->revents & POLLIN) {
				ready = 1;
				struct xdag_rpc_connection * conn = create_connection(p);
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
				p->fd = -1;
			}
		}

		if(!ready) {
			sleep(1);
			ready = 0;
		}
	}

	close(sock);
	free(g_fds);
	g_rpc_stop = 1;
	return 0;
}

/* start xdag rpc service */
static int xdag_rpc_service_init(int port)
{
	if(port > 0 && port < 65535) {
		g_rpc_port = port;
	} else {
		g_rpc_port = DEFAULT_RPC_PORT;
	}

	g_rpc_stop = 0;

	pthread_t th;
	int err = pthread_create(&th, NULL, rpc_service_thread, (void*)&g_rpc_port);
	if(err != 0) {
		printf("create rpc_service_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	
	err = pthread_detach(th);
	if(err != 0) {
		printf("detach rpc_service_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	xdag_rpc_command_host_add("127.0.0.1"); // always accept localhost
	
	/* init rpc procedures */
	xdag_rpc_init_procedures();
	
	return 0;
}

int xdag_rpc_service_start(int port)
{
	if(0 == g_rpc_stop) {
		printf("rpc service is running\n");
		return 1;
	}

	if(2 == g_rpc_stop) {
		printf("rpc service is stopping in progress\n");
		return 1;
	}

	return xdag_rpc_service_init(port);
}

/* stop xdag rpc service */
int xdag_rpc_service_stop(void)
{
	g_rpc_stop = 2;
	xdag_rpc_command_host_clear(); //clear all white list.
	return 0;
}
