/* httprpc, T13.670-T13.788 $DVS:time$ */

#ifndef XDAG_HTTP_RPC_H
#define XDAG_HTTP_RPC_H

extern int http_rpc_start(const char *username ,const char *passwd, int port);
extern int rpc_white_host_add(const char *host);
extern int rpc_wihte_host_del(const char *host);


#endif