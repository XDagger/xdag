#define WIN32_LEAN_AND_MEAN

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/time.h>
#include <termios.h>
#else
#include <Windows.h>
#include "../win/xdaglib/unistd.h"
#include <WinSock2.h>

#endif

#include <stdio.h>

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	uint64_t time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}

FILE *__iob_func(void)
{
	return stdin;
}

int tcgetattr(int fd, struct termios *tio)
{
	if(fd) return -1;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

#if !defined(_WIN32) && !defined(_WIN64)
	GetConsoleMode(hStdin, &tio->mode);
	tio->c_lflag = (tio->mode & ENABLE_ECHO_INPUT ? ECHO : 0);
#endif
	return 0;
}

int tcsetattr(int fd, int flags, struct termios *tio)
{
	if(fd || flags) return -1;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

#if !defined(_WIN32) && !defined(_WIN64)
	tio->mode = (tio->mode & ~ENABLE_ECHO_INPUT) | (tio->c_lflag & ECHO ? ENABLE_ECHO_INPUT : 0);
	SetConsoleMode(hStdin, tio->mode);
#endif
	return 0;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
	int a, b, c, d;
	if(sscanf(cp, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
	if((a | b | c | d) & ~0xFF) return 0;
	inp->s_addr = a | b << 8 | c << 16 | d << 24;
	return 1;
}
