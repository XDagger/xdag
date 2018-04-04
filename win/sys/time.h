#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <WinSock2.h>
#include <time.h>

extern int gettimeofday(struct timeval * tp, struct timezone * tzp);

#endif
