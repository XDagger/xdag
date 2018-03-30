//
//  rpc_procedures.c
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_procedures.h"
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
#include "rpc_wrapper.h"
#include "rpc_service.h"

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


///* account */
//cJSON * method_account(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id);
//cJSON * method_account(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id) {
//	char *result = NULL;
//	rpc_call_dnet_command("account", "", &result);
//	cJSON * ret = cJSON_CreateString(result);
//	if(result) {
//		free(result);
//	}
//	return ret;
//}

#define rpc_query_func(command) \
cJSON * method_##command (struct xdag_rpc_context * ctx, cJSON * params, cJSON *id); \
cJSON * method_##command (struct xdag_rpc_context * ctx, cJSON * params, cJSON *id) { \
	char *result = NULL; \
	rpc_call_dnet_command(#command, "", &result); \
	cJSON * ret = cJSON_CreateString(result); \
	if(result) { \
		free(result); \
	} \
	return ret; \
} 


#define rpc_register_func(command) \
xdag_rpc_service_register_procedure(&method_##command, #command, NULL);


rpc_query_func(account)
rpc_query_func(balance)
rpc_query_func(block)
rpc_query_func(miners)
rpc_query_func(pool)
rpc_query_func(state)
rpc_query_func(stats)
rpc_query_func(lastblocks)
rpc_query_func(xfer)


/* init rpc procedures */
int xdag_rpc_init_procedures(void)
{
	rpc_register_func(account);
	rpc_register_func(balance);
	rpc_register_func(block);
	rpc_register_func(miners);
	rpc_register_func(pool);
	rpc_register_func(state);
	rpc_register_func(stats);
	rpc_register_func(lastblocks);
	rpc_register_func(xfer);
	return 0;
}
