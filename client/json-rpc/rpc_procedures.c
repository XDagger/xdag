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
#include <ctype.h>

#include "rpc_wrapper.h"
#include "rpc_procedure.h"
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

#define rpc_register_func(command) xdag_rpc_service_register_procedure(&method_##command, #command, NULL);

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

/* method: xdag_version */
cJSON * method_xdag_version(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version);

/* method: xdag_get_account */
int rpc_account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key);
cJSON * method_xdag_get_account(struct xdag_rpc_context *ctx, cJSON *params, cJSON *id, char *version);

/* method: xdag_get_balance */
cJSON * method_xdag_get_balance(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version);

/* method: xdag_do_xfer */
cJSON * method_xdag_do_xfer(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version);

/* method: xdag_get_transactions */
int rpc_transactions_callback(void *data, int type, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time);
cJSON * method_xdag_get_transactions(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version);

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
 "result":[{"address":"ADDRESS", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"address":"ADDRESS", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 "version":"1.1", "result":[{"address":"ADDRESS", "balance":"10.111111", "key":"0"}], "error":null, "id":1
 */

struct rpc_account_callback_data {
	cJSON* root;
	int count;
};

int rpc_account_callback(void *data, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time, int n_our_key)
{
	struct rpc_account_callback_data *d = (struct rpc_account_callback_data*)data;
	if(d->count-- <=0) return -1;
	
	char address_buf[33];
	xdag_hash2address(hash, address_buf);

	cJSON* address = cJSON_CreateString(address_buf);
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
		sprintf(str, "%.9Lf", amount2xdags(balance));
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
 "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"ADDRESS"}], "id":1
 "jsonrpc":"2.0", "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"ADDRESS"}], "id":1
 "version":"1.1", "method":"xdag_do_xfer", "params":[{"amount":"1.0", "address":"ADDRESS"}], "id":1
 
 reponse:
 "result":[{"block":"ADDRESS"}], "error":null, "id":1
 "jsonrpc":"2.0", "result":[{"block":"ADDRESS"}], "error":null, "id":1
 "version":"1.1", "result":[{"block":"ADDRESS"}], "error":null, "id":1
 */

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

			char address_buf[33];
			xdag_hash2address(xfer.transactionBlockHash, address_buf);

			cJSON * ret = NULL;
			cJSON* item = cJSON_CreateObject();
			cJSON_AddItemToObject(item, "block", cJSON_CreateString(address_buf));
			
			ret = cJSON_CreateArray();
			cJSON_AddItemToArray(ret, item);
			return ret;
		}
		
		return NULL;
	}
}

/* transactions */
/*
 request:
 "method":"xdag_get_transactions", "params":[{"address":"ADDRESS", "page":0, "pagesize":50}], "id":1
 "jsonrpc":"2.0", "method":"xdag_get_account", "params":[{"address":"ADDRESS", "page":0, "pagesize":50}], "id":1
 "version":"1.1", "method":"xdag_get_account", "params":[{"address":"ADDRESS", "page":0, "pagesize":50}], "id":1
 
 reponse:
 "result":{"total": 1, "transactions":[{"direction":"input", "address":"ADDRESS", "amount":"10.111111", "timestamp":"2018-06-03 03:36:33.866 UTC"}]}, "error":null, "id":1
 "jsonrpc":"2.0", "result":{"total": 1, "transactions":[{"direction":"input", "address":"ADDRESS", "amount":"10.111111", "timestamp":"2018-06-03 03:36:33.866 UTC"}]}, "error":null, "id":1
 "version":"1.1", "result":{"total": 1, "transactions":[{"direction":"input", "address":"ADDRESS", "amount":"10.111111", "timestamp":"2018-06-03 03:36:33.866 UTC"}]}, "error":null, "id":1
 */

struct rpc_transactions_callback_data {
	cJSON* json_root;
	int page;
	int page_size;
	int count;
};

int rpc_transactions_callback(void *data, int type, xdag_hash_t hash, xdag_amount_t amount, xdag_time_t time)
{
	struct rpc_transactions_callback_data *callback_data = (struct rpc_transactions_callback_data*)data;
	
	++callback_data->count;
	
	if(callback_data->count < callback_data->page * callback_data->page_size) {
		return 0;
	}
	
	if(callback_data->count > (callback_data->page + 1) * callback_data->page_size) {
		return -1;
	}
	
	cJSON *json_direction = cJSON_CreateString(type ? "output" : "input");
	
	char address_buf[33];
	xdag_hash2address(hash, address_buf);
	cJSON* json_address = cJSON_CreateString(address_buf);
	
	char str[128] = {0};
	sprintf(str, "%.9Lf",  amount2xdags(amount));
	cJSON* json_amount = cJSON_CreateString(str);
	
	struct tm tm;
	char buf[64], tbuf[64];
	time_t t = time >> 10;
	localtime_r(&t, &tm);
	strftime(buf, 64, "%Y-%m-%d %H:%M:%S", &tm);
	sprintf(tbuf, "%s.%03d UTC", buf, (int)((time & 0x3ff) * 1000) >> 10);
	cJSON* json_time = cJSON_CreateString(tbuf);
	
	cJSON* json_item = cJSON_CreateObject();
	cJSON_AddItemToObject(json_item, "direction", json_direction);
	cJSON_AddItemToObject(json_item, "address", json_address);
	cJSON_AddItemToObject(json_item, "amount", json_amount);
	cJSON_AddItemToObject(json_item, "timestamp", json_time);
	
	cJSON_AddItemToArray(callback_data->json_root, json_item);
	return 0;
}

cJSON * method_xdag_get_transactions(struct xdag_rpc_context * ctx, cJSON * params, cJSON *id, char *version)
{
	xdag_mess("rpc call method get_transactions, version %s", version);
	
	char address[128] = {0};
	int page = 0;
	int pagesize = 50;
	
	if (params) {
		if (cJSON_IsArray(params) && cJSON_GetArraySize(params) == 1) {
			cJSON* param = cJSON_GetArrayItem(params, 0);
			if (!param || !cJSON_IsObject(param)) {
				ctx->error_code = 1;
				ctx->error_message = strdup("Invalid parameters.");
				return NULL;
			}
			
			cJSON * json_address = cJSON_GetObjectItem(param, "address");
			if (cJSON_IsString(json_address)) {
				strcpy(address, json_address->valuestring);
			} else {
				ctx->error_code = 1;
				ctx->error_message = strdup("Invalid address.");
				return NULL;
			}
			
			cJSON * json_page = cJSON_GetObjectItem(param, "page");
			if (cJSON_IsNumber(json_page)) {
				page = json_page->valueint;
			}
			
			cJSON * json_pagesize = cJSON_GetObjectItem(param, "pagesize");
			if (cJSON_IsNumber(json_pagesize)) {
				pagesize = json_pagesize->valueint;
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
	
	xdag_hash_t hash;
	int incorrect = 0;
	size_t len = strlen(address);
	
	if(len == 32) {
		if(xdag_address2hash(address, hash)) {
			ctx->error_code = 1;
			ctx->error_message = strdup("Address is incorrect.");
			return NULL;
		}
	} else if(len == 48 || len == 64) {
		for(int i = 0; i < len; ++i) {
			if(!isxdigit(address[i])) {
				ctx->error_code = 1;
				ctx->error_message = strdup("Hash is incorrect.");
				return NULL;
			}
		}
		if(!incorrect) {
			int c;
			for(int i = 0; i < 24; ++i) {
				sscanf(address + len - 2 - 2 * i, "%2x", &c);
				((uint8_t *)hash)[i] = c;
			}
		}
	} else {
		ctx->error_code = 1;
		ctx->error_message = strdup("Invalid parameters.");
		return NULL;
	}
	
	cJSON * array = cJSON_CreateArray();
	struct rpc_transactions_callback_data callback_data;
	callback_data.page = page;
	callback_data.page_size = pagesize;
	callback_data.json_root = array;
	callback_data.count = 0;
	
	int total = xdag_get_transactions(hash, &callback_data, &rpc_transactions_callback);
	if( total < 0) {
		ctx->error_code = 1;
		ctx->error_message = strdup("Block is not found.");
		cJSON_Delete(array);
		return NULL;
	}
	
	cJSON * result = cJSON_CreateObject();
	cJSON * json_total = cJSON_CreateNumber(total);
	cJSON_AddItemToObject(result, "total", json_total);
	cJSON_AddItemToObject(result, "transactions", array);
	
	return result;
}

/* init rpc procedures */
int xdag_rpc_init_procedures(void)
{
	/* register query func */
	rpc_register_func(xdag_version);
	rpc_register_func(xdag_state);
	rpc_register_func(xdag_stats);
	
	/* register xdag_get_account, xdag_get_balance, xdag_do_xfer, xdag_get_transactions */
	rpc_register_func(xdag_get_account);
	rpc_register_func(xdag_get_balance);
	rpc_register_func(xdag_do_xfer);
	rpc_register_func(xdag_get_transactions);
	return 0;
}
