#ifndef TERMIOS_H
#define TERMIOS_H

#include <stdint.h>

struct termios {
	uint32_t mode;
	uint32_t c_lflag;
};

#define ECHO	1
#define TCSANOW	0

extern int tcgetattr(int fd, struct termios *tio);
extern int tcsetattr(int fd, int flags, struct termios *tio);

#endif

