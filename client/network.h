#ifndef XDAG_NETWORK_H
#define XDAG_NETWORK_H

int xdag_network_init();
int xdag_connect_pool(const char* pool_address, char** error_message);
void xdag_connection_close(int socket);

#endif // XDAG_NETWORK_H
