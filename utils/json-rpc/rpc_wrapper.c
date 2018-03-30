//
//  rpc_wrapper.c
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_service.h"
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

#include "../../client/commands.h"
#include "../log.h"
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

void rpc_call_dnet_command(const char *method, const char *params, char **result)
{
	char *lasts;
	int sock;
	
	char cmd[XDAG_COMMAND_MAX];
	
	strcpy(cmd, method);
	char *ptr = strtok_r(cmd, " \t\r\n", &lasts);
	if (!ptr) {
		return;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't open unix domain socket errno:%d.\n", errno);
		return;
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK);
	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		xdag_err("Can't connect to unix domain socket errno:%d\n", errno);
		return;
	}
#else
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't create domain socket errno:%d", WSAGetLastError());
		return 0;
	}
	struct sockaddr_in addrLocal;
	addrLocal.sin_family = AF_INET;
	addrLocal.sin_port = htons(APPLICATION_DOMAIN_PORT);
	addrLocal.sin_addr.s_addr = htonl(LOCAL_HOST_IP);
	if (connect(sock, (struct sockadr*)&addrLocal, sizeof(addrLocal)) == -1) {
		xdag_err("Can't connect to domain socket errno:%d", errno);
		return 0;
	}
#endif
	write(sock, cmd, strlen(cmd) + 1);
	if (!strcmp(ptr, "terminate") || !strcmp(ptr, "exit")) {
		return;
	}
	
	*result = (char*)malloc(128);
	memset(*result, 0, 128);
	char c = 0;
	while (read(sock, &c, 1) == 1 && c) {
		size_t len = strlen(*result);
		if(len>=128) {
			*result = realloc(*result, len+128);
			memset(*result, len, 128);
		}
		sprintf(*result, "%s%c", *result, c);
	}
	close(sock);
}
