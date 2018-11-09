#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>

/* The following constants should be used for the second parameter of
`shutdown'.  */            
#define SHUT_RD     SD_RECEIVE /* No more receptions.  */    
#define SHUT_WR     SD_SEND    /* No more transmissions.  */               
#define SHUT_RDWR   SD_BOTH    /* No more receptions or transmissions.  */

#endif

