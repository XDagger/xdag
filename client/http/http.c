//
//  http.c
//  xdag
//
//  Created by Rui Xie on 5/25/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "http.h"
#include "url.h"
#include "../utils/log.h"

#include <signal.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(_WIN32) || defined(_WIN64)
#if defined(_WIN64)
#define poll WSAPoll
#else
#define poll(a, b, c)((a)->revents =(a)->events,(b))
#endif
#else
#include <poll.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#else
#include <netinet/in.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#endif


// Simple structure to keep track of the handle, and
// of what needs to be freed later.
typedef struct {
	int socket;
	SSL *sslHandle;
	SSL_CTX *sslContext;
} connection;

#define SERVER  "raw.githubusercontent.com"
#define PORT 443

connection * tcpConnect(const char* h, int port);
void tcpDisconnect(connection *c);
char* tcpRead(connection *c);
void tcpWrite(connection *c, char *text);

connection *sslConnect(const char* h, int port);
void sslDisconnect(connection *c);
char *sslRead(connection *c);
void sslWrite(connection *c, char *text);

// Establish a regular tcp connection
connection * tcpConnect(const char* h, int port)
{	
	int error, sock = 0 ;
	struct sockaddr_in server;
	if(!strcmp(h, "any")) {
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if(!inet_aton(h, &server.sin_addr)) {
		struct hostent *host = gethostbyname(h);
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if(sock == -1) {
			xdag_err("Create sock error, %s", strerror(sock));
			sock = 0;
		} else {
			server.sin_family = AF_INET;
			server.sin_port = htons(port);
			server.sin_addr = *((struct in_addr *) host->h_addr);
			bzero(&(server.sin_zero), 8);
			
			error = connect(sock,(struct sockaddr *) &server, sizeof(struct sockaddr));
			if(error == -1)
			{
				xdag_err("Connect error, %s", strerror(error));
				sock = 0;
			}
		}
	}
	
	if(sock) {
		connection *c = malloc(sizeof(connection));
		c->sslHandle = NULL;
		c->sslContext = NULL;
		c->socket = sock;
		return c;
	} else {
		return NULL;
	}
}

// Disconnect & free connection struct
void tcpDisconnect(connection *c)
{
	if(c->socket) {
		close(c->socket);
	}
	
	free(c);
}

char *tcpRead(connection *c)
{
	const int readSize = 1023;
	char *rc = NULL;
	size_t received, count = 0;
	char buffer[1024];
	
	if(c) {
		while(1) {
			if(!rc) {
				rc = malloc(readSize * sizeof(char) + 1);
			} else {
				rc = realloc(rc,(count + 1) * readSize * sizeof(char) + 1);
			}
			
			memset(rc,0,readSize + 1);
			received = recv(c->socket, buffer, 1, readSize);
			buffer[received] = '\0';
			
			if(received > 0) {
				strcat(rc, buffer);
			}
			
			if(received < readSize) {
				break;
			}
			count++;
		}
	}
	
	return rc;
}

void tcpWrite(connection *c, char *text)
{
	if(c) {
		write(c->socket, text, strlen(text));
	}
}

// Establish a connection using an SSL layer
connection *sslConnect(const char* h, int port)
{
	int error, sock = 0 ;
	struct sockaddr_in server;
	if(!strcmp(h, "any")) {
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if(!inet_aton(h, &server.sin_addr)) {
		struct hostent *host = gethostbyname(h);
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if(sock == -1) {
			xdag_err("Create sock error, %s", strerror(sock));
			sock = 0;
		} else {
			server.sin_family = AF_INET;
			server.sin_port = htons(port);
			server.sin_addr = *((struct in_addr *) host->h_addr);
			bzero(&(server.sin_zero), 8);
			
			error = connect(sock,(struct sockaddr *) &server, sizeof(struct sockaddr));
			if(error == -1)
			{
				xdag_err("Connect error, %s", strerror(error));
				sock = 0;
			}
		}
	}
	
	if(sock) {
		connection *c = malloc(sizeof(connection));
		c->sslHandle = NULL;
		c->sslContext = NULL;
		c->socket = sock;
		
		// Register the error strings for libcrypto & libssl
		SSL_load_error_strings();
		
		// Register the available ciphers and digests
		SSL_library_init();
		OpenSSL_add_all_algorithms();
		
		// New context saying we are a client, and using SSL 2 or 3
		c->sslContext = SSL_CTX_new(SSLv23_client_method());
		if(c->sslContext == NULL) {
			ERR_print_errors_fp(stderr);
		}
		
		// Create an SSL struct for the connection
		c->sslHandle = SSL_new(c->sslContext);
		if(c->sslHandle == NULL) {
			ERR_print_errors_fp(stderr);
		}
		
		// Connect the SSL struct to our connection
		if(!SSL_set_fd(c->sslHandle, c->socket)) {
			ERR_print_errors_fp(stderr);
		}
		
		// Initiate SSL handshake
		if(SSL_connect(c->sslHandle) != 1) {
			ERR_print_errors_fp(stderr);
		}
		
		return c;
	} else {
		xdag_err("Creat ssl sock failed.");
		return NULL;
	}
}

// Disconnect & free connection struct
void sslDisconnect(connection *c)
{
	if(c->socket) {
		close(c->socket);
	}
	
	if(c->sslHandle) {
		SSL_shutdown(c->sslHandle);
		SSL_free(c->sslHandle);
	}
	
	if(c->sslContext) {
		SSL_CTX_free(c->sslContext);
	}
	
	free(c);
}

// Read all available text from the connection
char *sslRead(connection *c)
{
	const int readSize = 1023;
	char *rc = NULL;
	int received, count = 0;
	char buffer[1024];
	
	if(c) {
		while(1) {
			if(!rc) {
				rc = malloc(readSize * sizeof(char) + 1);
			} else {
				rc = realloc(rc,(count + 1) * readSize * sizeof(char) + 1);
			}
			
			memset(rc,0,readSize + 1);
			received = SSL_read(c->sslHandle, buffer, readSize);
			buffer[received] = '\0';
			
			if(received > 0) {
				strcat(rc, buffer);
			}
			
			if(received < readSize) {
				break;
			}
			count++;
		}
	}
	
	return rc;
}

// Write text to the connection
void sslWrite(connection *c, char *text)
{
	if(c) {
		SSL_write(c->sslHandle, text, (int)strlen(text));
	}
}

// Very basic main: we send GET / and print the response.
int test_https(void)
{
	connection *c;
	char *response;
	
	c = sslConnect(SERVER, PORT);
	
	sslWrite(c, "GET /XDagger/xdag/master/client/netdb-white.txt HTTP/1.1\r\nHost: raw.githubusercontent.com\r\nconnection: close\r\n\r\n");
	response = sslRead(c);
	
	printf("[content] %s\n", response);
	
	sslDisconnect(c);
	free(response);
	
	return 0;
}

size_t http_get(const char *url, uint8_t *buffer)
{
	return 0;
}
