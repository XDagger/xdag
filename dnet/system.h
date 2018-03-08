#ifndef _SYSTEM_DNET_H
#define _SYSTEM_DNET_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <pthread.h>
#define inline __inline
#define __attribute__(x) 
typedef long ssize_t;
#define fcntl(a,b,c) 0
#define close closesocket
#ifdef _WIN32
#define sleep(x)			Sleep((x)*1000)
#else
#define sleep(x)			_sleep(x)
#endif
#define strtok_r strtok_s
#define localtime_r(a,b) localtime_s(b,a)
#define usleep(x) 0
#define write(a,b,c) send(a,b,c,0)
#define read(a,b,c) recv(a,b,c,0)
static pthread_t pthread_invalid;
extern int system_init(void);
#define strdup(x) _strdup(x)
#if defined(_MSC_VER)
#define SHUT_RDWR SD_BOTH
#endif
#else
#define pthread_invalid -1
#define system_init()	0
#define INVALID_SOCKET	-1
#endif

#endif
