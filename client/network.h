#ifndef XDAG_NETWORK_H
#define XDAG_NETWORK_H

int xdag_network_init(void);
int xdag_connect_pool(const char* pool_address, const char** error_message);
void xdag_connection_close(int socket);

#endif // XDAG_NETWORK_H
