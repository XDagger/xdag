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

#include "../commands.h"
#include "../utils/log.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

#if !defined(_WIN32) && !defined(_WIN64)
#define UNIX_SOCK  "unix_sock.dat"
#else
const uint32_t LOCAL_HOST_IP = 0x7f000001; // 127.0.0.1
const uint32_t APPLICATION_DOMAIN_PORT = 7676;
#endif

#define RESULT_SHIFT_SIZE 1024
void rpc_call_dnet_command(const char *method, const char *params, char **result)
{
	int sock;
	char cmd[XDAG_COMMAND_MAX];
	sprintf(cmd, "%s %s", method, params);

#if !defined(_WIN32) && !defined(_WIN64)
	if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't open unix domain socket errno:%d.\n", errno);
		return;
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK);
	if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		xdag_err("Can't connect to unix domain socket errno:%d\n", errno);
		return;
	}
#else
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't create domain socket errno:%d", WSAGetLastError());
		return 0;
	}
	struct sockaddr_in addrLocal;
	addrLocal.sin_family = AF_INET;
	addrLocal.sin_port = htons(APPLICATION_DOMAIN_PORT);
	addrLocal.sin_addr.s_addr = htonl(LOCAL_HOST_IP);
	if(connect(sock, (struct sockadr*)&addrLocal, sizeof(addrLocal)) == -1) {
		xdag_err("Can't connect to domain socket errno:%d", errno);
		return 0;
	}
#endif
	
	write(sock, cmd, strlen(cmd) + 1);
	
	*result = (char*)malloc(RESULT_SHIFT_SIZE);
	memset(*result, 0, RESULT_SHIFT_SIZE);
	char c = 0;
	while(read(sock, &c, 1) == 1 && c) {
		size_t len = strlen(*result);
		if(len>=RESULT_SHIFT_SIZE) {
			*result = realloc(*result, len + RESULT_SHIFT_SIZE);
			memset(*result + len, 0, RESULT_SHIFT_SIZE);
		}
		sprintf(*result, "%s%c", *result, c);
	}
	close(sock);
}
