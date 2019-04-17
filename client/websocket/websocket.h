//
//  websocket.h
//  xdag
//
//  Created by Rui Xie on 11/14/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef websocket_h
#define websocket_h

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
	extern int g_websocket_running;
	extern int g_websocket_port;
	extern int g_websocket_maxlimit; //max websocket clients

	extern void xdag_ws_message_append(char *message);
	extern int xdag_ws_server_start(int maxlimit, int port);
	extern int xdag_ws_server_stop(void);

#ifdef __cplusplus
};
#endif

#endif /* websocket_h */
