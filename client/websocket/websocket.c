//
//  websocket.c
//  xdag
//
//  Created by Rui Xie on 11/14/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "websocket.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#define poll WSAPoll
#else
#include <poll.h>
#endif

#include "wslay/wslay.h"
#include <openssl/sha.h>
#include "../utils/log.h"
#include "../utils/base64.h"
#include "../uthash/utlist.h"

#define WS_LOG_FILE "websocket.log"

#define ws_fatal(...) xdag_log(WS_LOG_FILE, XDAG_FATAL   , __VA_ARGS__)
#define ws_crit(...)  xdag_log(WS_LOG_FILE, XDAG_CRITICAL, __VA_ARGS__)
#define ws_err(...)   xdag_log(WS_LOG_FILE, XDAG_ERROR   , __VA_ARGS__)
#define ws_warn(...)  xdag_log(WS_LOG_FILE, XDAG_WARNING , __VA_ARGS__)
#define ws_mess(...)  xdag_log(WS_LOG_FILE, XDAG_MESSAGE , __VA_ARGS__)
#define ws_info(...)  xdag_log(WS_LOG_FILE, XDAG_INFO    , __VA_ARGS__)
#ifndef NDEBUG
#define ws_debug(...) xdag_log(WS_LOG_FILE, XDAG_DEBUG   , __VA_ARGS__)
#else
#define ws_debug(...)
#endif

#define WS_DEFAULT_PORT 9999
#define WS_DEFAULT_MAX_LIMIT 100

static pthread_mutex_t g_mutex_sessions = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex_messages = PTHREAD_MUTEX_INITIALIZER;
int g_websocket_running = 0;
int g_websocket_port = WS_DEFAULT_PORT;
int g_websocket_maxlimit = WS_DEFAULT_MAX_LIMIT;

typedef struct {
	int fd;
	struct in_addr addr;
	in_port_t port;
	struct pollfd pfd;
	wslay_event_context_ptr ctx;
} ws_session;

typedef struct ws_session_list_element_t {
	ws_session *session;
	struct ws_session_list_element_t *next;
} ws_session_list_element;
static ws_session_list_element *g_session_list = NULL;

typedef struct ws_message_list_element_t {
	char *message;
	struct ws_message_list_element_t *next;
} ws_message_list_element;
static ws_message_list_element *g_message_list = NULL;

// functions
void ws_cleanup(void);
void cleanup_threads(void *args);
int ws_session_init(ws_session *session);
int ws_session_communicate(ws_session *session);
void *ws_handler_thread(void *args);
void *ws_server_thread(void *args);

void xdag_ws_message_append(char *message)
{
	if (!g_websocket_running) {
		return;
	}
	ws_message_list_element *msg = malloc(sizeof(ws_message_list_element));
	memset(msg, 0, sizeof(ws_message_list_element));
	msg->message = strdup(message);
	pthread_mutex_lock(&g_mutex_messages);
	LL_APPEND(g_message_list, msg);
	pthread_mutex_unlock(&g_mutex_messages);
}

static ws_message_list_element* ws_first_message(void)
{
	ws_message_list_element *first = NULL;
	first = g_message_list;
	pthread_mutex_lock(&g_mutex_messages);
	if (first) {
		LL_DELETE(g_message_list, first);
	}
	pthread_mutex_unlock(&g_mutex_messages);
	return first;
}

static void ws_session_close(ws_session *session)
{
	ws_info("session from %s:%d closed\n", inet_ntoa(session->addr), ntohs(session->port));
	shutdown(session->fd, SHUT_RDWR);
	close(session->fd);
	free(session);
}

static void ws_session_append(ws_session *session)
{
	assert(session);

	ws_session_list_element *elem = NULL;
	int count = 0;

	pthread_mutex_lock(&g_mutex_sessions);
	LL_COUNT(g_session_list, elem, count);
	pthread_mutex_unlock(&g_mutex_sessions);

	if (count >= g_websocket_maxlimit) {
		ws_session_close(session);
	} else {
		elem = calloc(1, sizeof(ws_session_list_element));
		if (elem == NULL) {
			ws_err("[ws_session_append] alloca failed!");
			ws_session_close(session);
		} else {
			elem->session = session;
			ws_session_init(session); // init wslay websocket
			pthread_mutex_lock(&g_mutex_sessions);
			LL_APPEND(g_session_list, elem);
			pthread_mutex_unlock(&g_mutex_sessions);
			ws_info("%s connected\n", inet_ntoa(session->addr));
		}
	}
}

static void sha1(uint8_t *dst, const uint8_t *src, size_t src_length)
{
	SHA_CTX ctx;
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, src, src_length);
	SHA1_Final(dst, &ctx);
}

static void base64(uint8_t *dst, const uint8_t *src, size_t src_length)
{
	size_t outlen = 0;;
	char *outbuf = NULL;
	base64_encode(src, src_length, &outbuf, &outlen);
	memcpy(dst, outbuf, outlen);
	free(outbuf);
}

static int make_non_block(int fd)
{
	int flags, r;
	while((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR);
	if(flags == -1) {
		ws_err("fcntl set F_GETFL 0 failed!");
		return -1;
	}
	while((r = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR);
	if(r == -1) {
		ws_err("fcntl set F_SETFL O_NONBLOCK failed!");
		return -1;
	}
	return 0;
}

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" // UID from RFC6455

static void create_accept_key(char *dst, const char *client_key)
{
	uint8_t sha1buf[20], key_src[60];
	memcpy(key_src, client_key, 24);
	memcpy(key_src+24, WS_GUID, 36);
	sha1(sha1buf, key_src, sizeof(key_src));
	base64((uint8_t*)dst, sha1buf, 20);
	dst[BASE64_LENGTH(20)] = '\0';
}

static char* http_header_find_field_value(char *header, char *field_name, char *value)
{
	char *header_end,
	*field_start,
	*field_end,
	*next_crlf,
	*value_start;
	size_t field_name_len;

	/* Pointer to the last character in the header */
	header_end = header + strlen(header) - 1;
	field_name_len = strlen(field_name);
	field_start = header;

	do{
		field_start = strstr(field_start+1, field_name);
		field_end = field_start + field_name_len - 1;

		if(field_start != NULL
			&& field_start - header >= 2
			&& field_start[-2] == '\r'
			&& field_start[-1] == '\n'
			&& header_end - field_end >= 1
			&& field_end[1] == ':')
		{
			break; /* Found the field */
		} else {
			continue; /* This is not the one; keep looking. */
		}
	} while(field_start != NULL);

	if(field_start == NULL) {
		return NULL;
	}

	/* Find the field terminator */
	next_crlf = strstr(field_start, "\r\n");

	/* A field is expected to end with \r\n */
	if(next_crlf == NULL) {
		return NULL; /* Malformed HTTP header! */
	}

	/* If not looking for a value, then return a pointer to the start of values string */
	if(value == NULL) {
		return field_end+2;
	}

	value_start = strstr(field_start, value);

	/* Value not found */
	if(value_start == NULL) {
		return NULL;
	}

	/* Found the value we're looking for */
	if(value_start > next_crlf) {
		return NULL; /* but after the CRLF terminator of the field. */
	}

	/* The value we found should be properly delineated from the other tokens */
	if(isalnum(value_start[-1]) || isalnum(value_start[strlen(value)])) {
		return NULL;
	}

	return value_start;
}

static int http_handshake(int fd)
{
	/*
	 * Note: The implementation of HTTP handshake in this function is
	 * written for just a example of how to use of wslay library and is
	 * not meant to be used in production code.  In practice, you need
	 * to do more strict verification of the client's handshake.
	 */
	char header[16384], accept_key[29], *keyhdstart, *keyhdend, res_header[256];
	size_t header_length = 0, res_header_sent = 0, res_header_length;
	ssize_t r;
	while(1) {
		while((r = read(fd, header+header_length, sizeof(header)-header_length)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("read");
			return -1;
		} else if(r == 0) {
			ws_err("HTTP Handshake: Got EOF");
			return -1;
		} else {
			header_length += r;
			if(header_length >= 4 && memcmp(header+header_length-4, "\r\n\r\n", 4) == 0) {
				break;
			} else if(header_length == sizeof(header)) {
				ws_err("HTTP Handshake: Too large HTTP headers");
				return -1;
			}
		}
	}

	if(http_header_find_field_value(header, "Upgrade", "websocket") == NULL ||
		http_header_find_field_value(header, "Connection", "Upgrade") == NULL ||
		(keyhdstart = http_header_find_field_value(header, "Sec-WebSocket-Key", NULL)) == NULL) {
		fprintf(stderr, "HTTP Handshake: Missing required header fields");
		return -1;
	}
	for(; *keyhdstart == ' '; ++keyhdstart);
	keyhdend = keyhdstart;
	for(; *keyhdend != '\r' && *keyhdend != ' '; ++keyhdend);
	if(keyhdend-keyhdstart != 24) {
//		printf("%s\n", keyhdstart);
		ws_err("HTTP Handshake: Invalid value in Sec-WebSocket-Key");
		return -1;
	}
	create_accept_key(accept_key, keyhdstart);
	snprintf(res_header, sizeof(res_header),
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"\r\n", accept_key);
	res_header_length = strlen(res_header);
	while(res_header_sent < res_header_length) {
		while((r = write(fd, res_header + res_header_sent, res_header_length - res_header_sent)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("write");
			return -1;
		} else {
			res_header_sent += r;
		}
	}
	return 0;
}

static void server_error_exit(const char *message, int sock)
{
	shutdown(sock, SHUT_RD);
	printf("Server experienced an error: \t%s\nShutting down ...\n", message);
	fflush(stdout);
	close(sock);
	exit(EXIT_FAILURE);
}

static ssize_t send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data)
{
	ws_session *session = (ws_session *)user_data;
	ssize_t r;
	int sflags = 0;
#ifdef MSG_MORE
	if(flags & WSLAY_MSG_MORE) {
		sflags |= MSG_MORE;
	}
#endif // MSG_MORE
	while((r = send(session->fd, data, len, sflags)) == -1 && errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	}
	return r;
}

static ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, int flags, void *user_data)
{
	ws_session *session = (ws_session *)user_data;
	ssize_t r;
	while((r = recv(session->fd, buf, len, 0)) == -1 && errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	} else if(r == 0) {
		/* Unexpected EOF is also treated as an error */
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		r = -1;
	}
	return r;
}

static void on_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data)
{
	/* Echo back non-control message */
	if(!wslay_is_ctrl_frame(arg->opcode)) {
		struct wslay_event_msg msgarg = {
			arg->opcode, arg->msg, arg->msg_length
		};
		wslay_event_queue_msg(ctx, &msgarg);
	}
}

void ws_cleanup(void)
{
	pthread_mutex_lock(&g_mutex_sessions);
	ws_session_list_element *elem1, *tmp1;
	LL_FOREACH_SAFE(g_session_list, elem1, tmp1)
	{
		LL_DELETE(g_session_list, elem1);
		if(elem1->session) {
			ws_session_close(elem1->session);
		}
		free(elem1);
	}
	pthread_mutex_unlock(&g_mutex_sessions);

	pthread_mutex_lock(&g_mutex_messages);
	ws_message_list_element *elem2, *tmp2;
	LL_FOREACH_SAFE(g_message_list, elem2, tmp2)
	{
		LL_DELETE(g_message_list, elem2);
		if(elem2->message) {
			free(elem2->message);
		}
		free(elem2);
	}
	pthread_mutex_unlock(&g_mutex_messages);
}

void cleanup_threads(void *args)
{
	//TODO: add thread cleanup code
	return;
}

static struct wslay_event_callbacks callbacks = {
	recv_callback,
	send_callback,
	NULL,
	NULL,
	NULL,
	NULL,
	on_msg_recv_callback
};

int ws_session_init(ws_session *session)
{
	ws_info("session init");
	session->pfd.fd = session->fd;
	session->pfd.events = POLLIN|POLLOUT;

	if(http_handshake(session->fd) == -1) {
		return -1;
	}

	if(make_non_block(session->fd) == -1) {
		return -1;
	}

	int val = 1;
	if(setsockopt(session->fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val)) == -1) {
		perror("setsockopt: TCP_NODELAY");
		return -1;
	}

	wslay_event_context_server_init(&session->ctx, &callbacks, session);

	return 0;
}

void *ws_handler_thread(void *args)
{
	pthread_cleanup_push(cleanup_threads, args);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	ws_message_list_element *msg_elem = NULL;
	ws_session_list_element *session_elem = NULL, *tmp = NULL;
	char *message = NULL;
	ws_session *session = NULL;

	struct pollfd * fds = calloc(g_websocket_maxlimit, sizeof(struct pollfd));

	while (g_websocket_running) {
		msg_elem = ws_first_message();
		if(msg_elem) {
			if(msg_elem->message) {
				message = msg_elem->message;
				pthread_mutex_lock(&g_mutex_sessions);
				LL_FOREACH(g_session_list, session_elem)
				{
					session = session_elem->session;
					struct wslay_event_msg msgarg = {
						1, (uint8_t*)message, strlen(message)
					};
					wslay_event_queue_msg(session->ctx, &msgarg);
				}
				pthread_mutex_unlock(&g_mutex_sessions);

				free(msg_elem->message);
			}
			free(msg_elem);
		}

		int count = 0;
		pthread_mutex_lock(&g_mutex_sessions);
		LL_COUNT(g_session_list, session_elem, count);
		pthread_mutex_unlock(&g_mutex_sessions);

		assert(count <= g_websocket_maxlimit);//check

		int index = 0;
		struct pollfd *fd = NULL;

		pthread_mutex_lock(&g_mutex_sessions);
		LL_FOREACH(g_session_list, session_elem)
		{
			if(index >= g_websocket_maxlimit) { //skip invalid sessions
				continue;
			}

			fd = fds + index;
			session = session_elem->session;

			if(wslay_event_want_read(session->ctx)) {
				session->pfd.events |= POLLIN;
			}
			if(wslay_event_want_write(session->ctx)) {
				session->pfd.events |= POLLOUT;
			}

			memcpy(fd, &session->pfd, sizeof(struct pollfd));
			++index;
		}
		pthread_mutex_unlock(&g_mutex_sessions);

		// poll
		int res = poll(fds, g_websocket_maxlimit, 0);
		if (res == -1 && errno == EINTR) {
			continue;
		}

		if (res == -1) {
			ws_err("[ws_handle_thread] poll error, err : %s", strerror(errno));
			continue;
		}

		index = 0;
		fd = NULL;
		res = 0;
		int processed = 0;
		pthread_mutex_lock(&g_mutex_sessions);
		LL_FOREACH_SAFE(g_session_list, session_elem, tmp)
		{
			fd = fds + index;
			session = session_elem->session;

			if(fd->revents & POLLIN){
				if(wslay_event_recv(session->ctx) != 0) {
					ws_err("read error\n");
					res = -1;
				} else {
					processed = 1;
				}
			}

			if(fd->revents & POLLOUT){
				if(wslay_event_send(session->ctx) != 0) {
					ws_err("write error\n");
					res = -1;
				} else {
					processed = 1;
				}
			}

			if(fd->revents & POLLERR) {
				ws_err("POLLERR\n");
				res = -1;
			}

			if(fd->revents & POLLHUP) {
				ws_err("POLLHUP\n");
				res = -1;
			}

			if(fd->revents & POLLNVAL) {
				ws_err("POLLNVAL\n");
				res = -1;
			}

			if(res != 0) {
				LL_DELETE(g_session_list, session_elem);
				if(session_elem == g_session_list) {
					g_session_list = NULL;
				}
				ws_session_close(session);
				free(session_elem);
			}
			++index;
		}
		pthread_mutex_unlock(&g_mutex_sessions);

		if(!processed) {
			sleep(1); // sleep 1 if nothing processed
		}
	}

	g_websocket_running = 0;
	ws_cleanup();

	pthread_cleanup_pop(0);
	pthread_exit((void *) EXIT_SUCCESS);
	return 0;
}

void *ws_server_thread(void *args)
{
	pthread_cleanup_push(cleanup_threads, args);

	int port = g_websocket_port;
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0) {
		server_error_exit(strerror(errno), sock);
	}

	int on = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		server_error_exit(strerror(errno), sock);
	}

	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	memset(&peeraddr, 0, sizeof(struct sockaddr_in));
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	peeraddr.sin_port = htons(port);

	if(bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr)) < 0) {
		server_error_exit(strerror(errno), sock);
	}

	if(listen(sock, 100) < 0) {
		server_error_exit(strerror(errno), sock);
	}

	while(g_websocket_running) {
		int peer_sock = accept(sock, (struct sockaddr*)&peeraddr, &peeraddr_len);
		if(peer_sock < 0) {
			ws_err("accept error : %s\n", strerror(errno));
			continue;
		}

		ws_session *session = calloc(1, sizeof(ws_session));
		if (!session) {
			ws_err("[ws_server_thread] calloc failed!");
			continue;
		}
		session->fd = peer_sock;
		session->addr = peeraddr.sin_addr;
		session->port = peeraddr.sin_port;

		ws_session_append(session);// append session to session list
	}

	close(sock);

	pthread_cleanup_pop(0);
	pthread_exit((void *) EXIT_SUCCESS);
	return 0;
}

int xdag_ws_server_start(int maxlimit, int port)
{
	g_websocket_running = 1;
	if (maxlimit < 0 || maxlimit > 65535) { // set to default max limit
		ws_warn("Invalid websocket max limit value, set to default value %d", WS_DEFAULT_MAX_LIMIT);
		g_websocket_maxlimit = WS_DEFAULT_MAX_LIMIT;
	}

	if (port < 0 || port > 65535) { // set to default websocket port
		ws_warn("Invalid websocket port, set to default port %d", WS_DEFAULT_PORT);
		g_websocket_port = WS_DEFAULT_PORT;
	}

	pthread_t pthread_id;
	pthread_attr_t pthread_attr;

	// create handler thread
	pthread_attr_init(&pthread_attr);
	pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	//pthread_attr_setstacksize(&pthread_attr, 524288);
	if(pthread_create(&pthread_id, &pthread_attr, ws_handler_thread, NULL) < 0) {
		ws_err("ws_handler_thread create failed due to error : %s\n", strerror(errno));
		pthread_attr_destroy(&pthread_attr);
		return -1;
	}
	pthread_attr_destroy(&pthread_attr);

	// create server thread
	pthread_attr_init(&pthread_attr);
	pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	//pthread_attr_setstacksize(&pthread_attr, 524288);
	if(pthread_create(&pthread_id, &pthread_attr, ws_server_thread, NULL) < 0) {
		ws_err("ws_server_thread create failed due to error : %s\n", strerror(errno));
		pthread_attr_destroy(&pthread_attr);
		return -1;
	}
	pthread_attr_destroy(&pthread_attr);

	return 0;
}
