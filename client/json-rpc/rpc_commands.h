//
//  rpc_commands.h
//  xdag
//
//  Created by Rui Xie on 11/2/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef rpc_commands_h
#define rpc_commands_h

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

	struct sockaddr_in;

	/* rpc commands */
	extern int xdag_rpc_command(const char *, FILE *);

	extern int xdag_rpc_command_host_check(struct sockaddr_in);
	extern int xdag_rpc_command_host_add(const char *);
	extern int xdag_rpc_command_host_del(const char *);
	extern void xdag_rpc_command_host_clear(void);
	extern void xdag_rpc_command_host_query(char *);
	extern void xdag_rpc_command_list_methods(char *);
	extern void xdag_rpc_command_disable_xfer(void);
	extern void xdag_rpc_command_enable_xfer(void);
	extern void xdag_rpc_command_help(FILE *);

#ifdef __cplusplus
};
#endif

#endif /* rpc_commands_h */
