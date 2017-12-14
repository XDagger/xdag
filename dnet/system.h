#ifndef _SYSTEM_H
#define _SYSTEM_H

#ifdef _WIN32
#include <pthread.h>
#define inline __inline
#define __attribute__(x) 
typedef long ssize_t;
#define fcntl(a,b,c) 0
#define close closesocket
#define sleep _sleep
#define strtok_r strtok_s
#define localtime_r(a,b) localtime_s(b,a)
#define usleep(x) 0
#define write(a,b,c) send(a,b,c,0)
#define read(a,b,c) recv(a,b,c,0)
static pthread_t pthread_invalid;
extern int system_init(void);
#else
#define pthread_invalid -1
#define system_init() 0
#endif

#endif
