//
//  rpc_procedures.c
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "rpc_procedures.h"
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
#include <pthread.h>

#include "cJSON.h"
#include "cJSON_Utils.h"
#include "rpc_wrapper.h"
#include "rpc_service.h"
#include "../version.h"
#include "../init.h"
#include "../block.h"
#include "../address.h"
#include "../commands.h"
#include "../wallet.h"
#include "../../dus/programs/dfstools/source/dfslib/dfslib_random.h"
#include "../../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../../dus/programs/dfstools/source/dfslib/dfslib_string.h"
#include "../../dnet/dnet_main.h"
#include "../utils/log.h"

#define rpc_query_func(command) \
cJSON * method_xdag_##command (struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version); \
cJSON * method_xdag_##command (struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version) { \
	xdag_debug("rpc call method %s, version %s", #command, version); \
	char *result = NULL; \
	rpc_call_dnet_command(#command, "", &result); \
	cJSON * ret = cJSON_CreateString(result); \
	if(result) { \
		free(result); \
	} \
	return ret; \
}

rpc_query_func(state)
rpc_query_func(stats)

/* version */
/*
 request:
 "method":"xdag_version", "params":[], "id":1
 "jsonrpc":"2.0", "method":"xdag_version", "params":[], "id":1
 "version":"1.1", "method":"xdag_version", "params":[], "id":1
 
 reponse:
 "result":[{"version":"0.2.1"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"version":"0.2.1"}], "error":null, "id":1
 "version":"1.1", "result":[{"version":"0.2.1"}], "error":null, "id":1
 */
cJSON * method_xdag_version(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version);
cJSON * method_xdag_version(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version)
{
	xdag_debug("rpc call method version, version %s",version);
	cJSON * ret = NULL;
	cJSON* item = cJSON_CreateObject();
	
	cJSON_AddItemToObject(item, "version", cJSON_CreateString(XDAG_VERSION));
	
	ret = cJSON_CreateArray();
	cJSON_AddItemToArray(ret, item);
	return ret;
}

/* account */
/*
 request:
 "method":"xdag_get_account", "params":["N"], "id":1
 "jsonrpc":"2.0", "method":"xdag_get_account", "params":["N"], "id":1
 "version":"1.1", "method":"xdag_get_account", "params":["N"], "id":1
 
 reponse:
 "result":[{"address":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"address":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 "version":"1.1", "result":[{"address":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 */

struct rpc_account_callback_data {
	cJSON* root;
	int count;
};

int rpc_account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key);
int rpc_account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	struct rpc_account_callback_data *d = (struct rpc_account_callback_data*)data;
	if(d->count-- <=0) return -1;
	
	cJSON* address = cJSON_CreateString(xdag_hash2address(hash));
	char str[128] = {0};
	sprintf(str, "%.9Lf",  amount2xdags(amount));
	cJSON* balance = cJSON_CreateString(str);
	sprintf(str, "%d", n_our_key);
	cJSON* key = cJSON_CreateString(str);
	
	cJSON* item = cJSON_CreateObject();
	cJSON_AddItemToObject(item, "address", address);
	cJSON_AddItemToObject(item, "balance", balance);
	cJSON_AddItemToObject(item, "key", key);
	
	cJSON_AddItemToArray(d->root, item);
	return 0;
}

cJSON * method_xdag_get_account(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version);
cJSON * method_xdag_get_account(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version)
{
	xdag_debug("rpc call method get_account, version %s",version);
	struct rpc_account_callback_data cbdata;
	cbdata.count = (g_is_miner ? 1 : 20);
	if (params) {
		if (cJSON_IsArray(params)) {
			size_t size = cJSON_GetArraySize(params);
			int i = 0;
			for (i = 0; i < size; i++) {
				cJSON* item = cJSON_GetArrayItem(params, i);
				if(cJSON_IsString(item)) {
					cbdata.count = atoi(item->valuestring);
					break;
				}
			}
		} else {
			ctx->error_code = 1;
			ctx->error_message = strdup("Invalid parameters.");
		}
	}
		
	cJSON * ret = NULL;
	if(ctx->error_code == 0) {
		if(g_xdag_state < XDAG_STATE_XFER) {
			ctx->error_code = 1;
			ctx->error_message = strdup("Not ready to show a balance.");
		} else {
			ret = cJSON_CreateArray();
			cbdata.root = ret;
			xdag_traverse_our_blocks(&cbdata, &rpc_account_callback);
		}
	}
	
	return ret;
}


/* balance */
/*
 request:
 "method":"xdag_get_balance", "params":["A"], "id":1
 "jsonrpc":"2.0", "method":"xdag_get_balance", "params":["A"], "id":1
 "version":"1.1", "method":"xdag_get_balance", "params":["A"], "id":1
 
 reponse:
 "result":[{"balance":"10.111111"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"balance":"10.111111"}], "error":null, "id":1
 "version":"1.1", "result":[{"balance":"10.111111"}], "error":null, "id":1
 */
cJSON * method_xdag_get_balance(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version);
cJSON * method_xdag_get_balance(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version)
{
	xdag_debug("rpc call method get_balance, version %s", version);
	char address[128] = {0};
	if (params) {
		if (cJSON_IsArray(params)) {
			size_t size = cJSON_GetArraySize(params);
			int i = 0;
			for (i = 0; i < size; i++) {
				cJSON* item = cJSON_GetArrayItem(params, i);
				if(cJSON_IsString(item)) {
					strcpy(address, item->valuestring);
					break;
				}
			}
		} else {
			ctx->error_code = 1;
			ctx->error_message = strdup("Invalid parameters.");
			return NULL;
		}
	}
	
	if(g_xdag_state < XDAG_STATE_XFER) {
		ctx->error_code = 1;
		ctx->error_message = strdup("Not ready to show a balance.");
		return NULL;
	} else {
		cJSON * ret = NULL;
		cJSON* item = cJSON_CreateObject();
		xdag_hash_t hash;
		xdag_amount_t balance;
		
		if(strlen(address)) {
			xdag_address2hash(address, hash);
			balance = xdag_get_balance(hash);
		} else {
			balance = xdag_get_balance(0);
		}
		
		char str[128] = {0};
		sprintf(str, "%.9Lf",  xdag_amount2xdag(balance) + (long double)xdag_amount2cheato(balance) / 1000000000);
		cJSON_AddItemToObject(item, "balance", cJSON_CreateString(str));
		
		ret = cJSON_CreateArray();
		cJSON_AddItemToArray(ret, item);
		return ret;
	}
	
	return NULL;
}


/* xfer */
/*
 request:
 "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"AAAAAAAAAAAAAAAAA"}], "id":1
 "jsonrpc":"2.0", "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"AAAAAAAAAAAAAAAAA"}], "id":1
 "version":"1.1", "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"AAAAAAAAAAAAAAAAA"}], "id":1
 
 reponse:
 "result":[{"block":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"block":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}], "error":null, "id":1
 "version":"1.1", "result":[{"block":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}], "error":null, "id":1
 */

cJSON * method_xdag_do_xfer(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version);
cJSON * method_xdag_do_xfer(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version)
{
	//todo: need password or not?
	xdag_debug("rpc call method do_xfer, version %s", version);
	
	char amount[128] = {0};
	char address[128] = {0};
	
	if (params) {
		if (cJSON_IsArray(params) && cJSON_GetArraySize(params) == 1) {
			cJSON* param = cJSON_GetArrayItem(params, 0);
			if (!param || !cJSON_IsObject(param)) {
				ctx->error_code = 1;
				ctx->error_message = strdup("Invalid parameters.");
				return NULL;
			}
			
			cJSON * json_amount = cJSON_GetObjectItem(param, "amount");
			if (cJSON_IsString(json_amount)) {
				strcpy(amount, json_amount->valuestring);
			}
			
			cJSON * json_address = cJSON_GetObjectItem(param, "address");
			if (cJSON_IsString(json_address)) {
				strcpy(address, json_address->valuestring);
			}
		} else {
			ctx->error_code = 1;
			ctx->error_message = strdup("Invalid parameters.");
			return NULL;
		}
	} else {
		ctx->error_code = 1;
		ctx->error_message = strdup("Invalid parameters.");
		return NULL;
	}
	
	if(g_xdag_state < XDAG_STATE_XFER) {
		ctx->error_code = 1;
		ctx->error_message = strdup("Not ready to transfer.");
		return NULL;
	} else {					
		struct xfer_callback_data xfer;
		
		memset(&xfer, 0, sizeof(xfer));
		xfer.remains = xdags2amount(amount);
		if(!xfer.remains) {
			ctx->error_code = 1;
			ctx->error_message = strdup("Xfer: nothing to transfer.");
		} else if(xfer.remains > xdag_get_balance(0)) {
			ctx->error_code = 1;
			ctx->error_message = strdup("Xfer: balance too small.");
		} else if(xdag_address2hash(address, xfer.fields[XFER_MAX_IN].hash)) {
			ctx->error_code = 1;
			ctx->error_message = strdup("Xfer: incorrect address.");
		} else {
			xdag_wallet_default_key(&xfer.keys[XFER_MAX_IN]);
			xfer.outsig = 1;
			g_xdag_state = XDAG_STATE_XFER;
			g_xdag_xfer_last = time(0);
			xdag_traverse_our_blocks(&xfer, &xfer_callback);
			
			cJSON * ret = NULL;
			cJSON* item = cJSON_CreateObject();				
			cJSON_AddItemToObject(item, "block", cJSON_CreateString(xdag_hash2address(xfer.transactionBlockHash)));
			
			ret = cJSON_CreateArray();
			cJSON_AddItemToArray(ret, item);
			return ret;
		}
		
		return NULL;
	}
}

#define rpc_register_func(command) xdag_rpc_service_register_procedure(&method_##command, #command, NULL);

/* init rpc procedures */
int xdag_rpc_init_procedures(void)
{
	/* register query func */
	rpc_register_func(xdag_version);
	rpc_register_func(xdag_state);
	rpc_register_func(xdag_stats);
	
	/* register xdag_get_account, xdag_get_balance, xdag_do_xfer */
	rpc_register_func(xdag_get_account);
	rpc_register_func(xdag_get_balance);
	rpc_register_func(xdag_do_xfer);
	return 0;
}
