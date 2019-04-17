#ifndef _SYSTEM_DNET_H
#define _SYSTEM_DNET_H

#ifdef _WIN32
#include <windows.h>
#include <pthread.h>
#define inline __inline
#define __attribute__(x) 
typedef long ssize_t;
#define fcntl(a,b,c) 0
#define close closesocket
#define strtok_r strtok_s
#define localtime_r(a,b) localtime_s(b,a)
#define usleep(x) 0
static pthread_t pthread_invalid;
#define strdup(x) _strdup(x)
#if defined(_MSC_VER)
#define SHUT_RDWR SD_BOTH
#endif
#else
#define pthread_invalid -1
#define INVALID_SOCKET	-1
#endif

#endif
