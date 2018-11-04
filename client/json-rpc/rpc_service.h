//
//  rpc_service.h
//  xdag
//
//  Created by Rui Xie on 3/29/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef XDAG_RPC_SERVICE_H
#define XDAG_RPC_SERVICE_H

#include "cJSON.h"
#include "cJSON_Utils.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_rpc_stop;
extern int g_rpc_port;
extern int g_rpc_xfer_enable;
extern int g_rpc_white_enable;

/* init xdag rpc */
extern int xdag_rpc_service_start(int port);

/* stop xdag rpc */
extern int xdag_rpc_service_stop(void);

#ifdef __cplusplus
};
#endif

#endif //XDAG_RPC_SERVICE_H
