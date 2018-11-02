//
//  rpc_commands.c
//  xdag
//
//  Created by Rui Xie on 11/2/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_commands.h"
#include "../uthash/utlist.h"
#include "../utils/log.h"
#include "../utils/utils.h"
#include "rpc_service.h"

#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define RPC_WHITE_ADDR_LEN          64
#define RPC_WHITE_MAX               16

typedef struct rpc_white_element {
	struct rpc_white_element *prev, *next;
	struct in_addr addr;
} rpc_white_element;
struct rpc_white_element *g_rpc_white_host = NULL;

int xdag_rpc_command_host_check(struct sockaddr_in peeraddr)
{
	rpc_white_element *element = NULL ;

	if(!g_rpc_white_host){
		return 1;
	}

	LL_FOREACH(g_rpc_white_host,element)
	{
		if(element->addr.s_addr == peeraddr.sin_addr.s_addr){
			return 1;
		}
	}

	return 0;
}

int xdag_rpc_command_host_add(const char *host)
{
	rpc_white_element *new_white_host = NULL, *tmp = NULL;
	int white_num = 0;
	struct in_addr addr = {0};

	if(!validate_ipv4(host)) {
		xdag_err("ip address is invalid");
		return -1;
	}

	LL_COUNT(g_rpc_white_host, new_white_host, white_num);
	if(white_num >= RPC_WHITE_MAX) {
		xdag_err("white list number is up to maximum");
		return -2;
	}

	addr.s_addr = inet_addr(host);
	LL_FOREACH_SAFE(g_rpc_white_host, new_white_host, tmp)
	{
		if(new_white_host->addr.s_addr == addr.s_addr){
			xdag_warn("host [%s] is in the rpc white list.",host);
			return -3;
		}
	}

	new_white_host = malloc(sizeof(rpc_white_element));
	if(NULL == new_white_host){
		xdag_err("memory is not enough.");
		return -4;
	}

	new_white_host->addr.s_addr = addr.s_addr;
	DL_APPEND(g_rpc_white_host, new_white_host);

	return 0;
}

int xdag_rpc_command_host_del(const char *host)
{
	rpc_white_element *node = NULL ,*tmp = NULL,  *del_node = NULL;
	struct in_addr addr = {0};

	LL_FOREACH_SAFE(g_rpc_white_host, node, tmp)
	{
		addr.s_addr = inet_addr(host);
		if(addr.s_addr == node->addr.s_addr){
			del_node = node;
			LL_DELETE(g_rpc_white_host, del_node);
			free(del_node);
			return 0;
		}
	}

	return -1;
}

void xdag_rpc_command_host_clear(void)
{
	rpc_white_element *node = NULL ,*tmp = NULL;
	LL_FOREACH_SAFE(g_rpc_white_host, node, tmp)
	{
		LL_DELETE(g_rpc_white_host, node);
		free(node);
	}
	return;
}

void xdag_rpc_command_host_query(char *result)
{
	rpc_white_element *element = NULL, *tmp = NULL;
	char new_host[RPC_WHITE_ADDR_LEN] = {0};

	LL_FOREACH_SAFE(g_rpc_white_host,element,tmp)
	{
		memset(new_host, 0, RPC_WHITE_ADDR_LEN);
		sprintf(new_host, "%s\n",inet_ntoa(element->addr));
		strcat(result, new_host);
	}
}

void xdag_rpc_command_help(FILE *out)
{
	fprintf(out,"Commands:\n");
	fprintf(out,"  list                 - list white hosts\n");
	fprintf(out,"  add IP               - add IP to white hosts, max number of white hosts is 16\n");
	fprintf(out,"  del IP               - delete IP from white hosts\n");
	fprintf(out,"  clear                - clear white hosts\n");
	fprintf(out,"  help                 - print this help\n");
}

int xdag_rpc_command(const char *cmd, FILE *out)
{
	char buf[4096], *nextParam;
	strcpy(buf, cmd);

	char *method = strtok_r(buf, " \t\r\n", &nextParam);
	if(!method) {
		if(0 == g_rpc_stop) {
			fprintf(out, "rpc service is running at port : %d.\n", g_rpc_port);
		} else if(1 == g_rpc_stop) {
			fprintf(out, "rpc service not started.\n");
		} else if(2 == g_rpc_stop) {
			fprintf(out, "rpc service is stopping in progress.\n");
		}
		return 0;
	}

	if(!strcmp(method, "stop")) {
		xdag_rpc_service_stop();
		return 0;
	} else if(!strcmp(method, "start")) {
		char *sport = strtok_r(0, " \t\r\n", &nextParam);
		int port = 0;
		if(sport && sscanf(sport, "%d", &port) != 1) {
			fprintf(out, "illegal port\n");
			return -1;
		}
		if(!xdag_rpc_service_start(port)) {
			fprintf(out, "start rpc at port : %d\n", g_rpc_port);
		} else {
			fprintf(out, "start rpc failed.\n");
		}
	} else if(!strcmp(method, "list")) {
		char list[RPC_WHITE_MAX * RPC_WHITE_ADDR_LEN] = {0};
		xdag_rpc_command_host_query(list);
		fprintf(out, "%s", list);
	} else if(!strcmp(method, "add")) {
		char *address = strtok_r(0, " \t\r\n", &nextParam);
		if(!address) {
			fprintf(out, "rpc: address not given.\n");
			return -1;
		}
		int ret = xdag_rpc_command_host_add(address);
		switch(ret){
			case 0:
				break;
			case -1:
				fprintf(out, "add [%s] failed:address is invalid \n", address);
				break;
			case -2:
				fprintf(out, "add [%s] failed:only allowed 8 white address \n", address);
				break;
			default:
				fprintf(out, "add [%s] failed:system error ,try again later\n", address);
				break;
		}
	} else if(!strcmp(method, "del")) {
		char *address = strtok_r(0, " \t\r\n", &nextParam);
		if(!address) {
			fprintf(out, "rpc: address not given.\n");
			return 0;
		}
		xdag_rpc_command_host_del(address);
	} else if(!strcmp(method, "clear")) {
		xdag_rpc_command_host_clear();
	} else {
		xdag_rpc_command_help(out);
	}

	return 0;
}
