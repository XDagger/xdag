/* httprpc, T13.670-T13.788 $DVS:time$ */

#ifndef XDAG_HTTP_RPC_H
#define XDAG_HTTP_RPC_H

extern int http_rpc_start(const char *username ,const char *passwd, int port);
extern int http_rpc_command(void *out, char *type, const char *address);

#endif