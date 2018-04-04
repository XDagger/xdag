#ifndef _INET_H
#define _INET_H

#include <winsock2.h>
#include <ws2tcpip.h>

int inet_aton(const char *cp, struct in_addr *inp);

#endif