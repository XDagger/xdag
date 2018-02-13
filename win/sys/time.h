#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <time.h>
#include <WinSock2.h>

extern int gettimeofday(struct timeval * tp, struct timezone * tzp);

#endif

