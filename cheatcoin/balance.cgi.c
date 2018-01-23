#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_IN		"/home/ec2-user/.cheatcoin1/fifo_cmd.dat"
#define FIFO_OUT	"/home/ec2-user/.cheatcoin1/fifo_res.dat"

int main(void) {
	char *var, cmd[256], *p, *q;
	int fd, c = 0;
	var = getenv("REQUEST_METHOD");
	if (!var || strcmp(var, "GET")) goto page;
	var = getenv("QUERY_STRING");
	if (!var || var[0] != 'a' || var[1] != '=' || !var[2] || isspace((unsigned char)var[2])) goto page;
	fd = open(FIFO_IN, O_WRONLY);
	if (fd < 0) goto page;
	snprintf(cmd, 256, "balance %s", var + 2);
	for (p = q = cmd; *p; ++p, ++q) {
		if (*p == '%') {
			char x[3]; unsigned y;
			x[0] = p[1], x[1] = p[2], x[2] = 0; p += 2;
			sscanf(x, "%x", &y);
			*q = y;
		} else *q = *p;
	}
	*q = 0;
	write(fd, cmd, strlen(cmd) + 1);
	close(fd);
	fd = open(FIFO_OUT, O_RDONLY);
	if (fd < 0) goto page;
	printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");
	while (read(fd, &c, 1) == 1 && c) putchar(c);
	close(fd);
	return 0;
page:
	printf("Content-Type: text/html; charset=utf-8\r\n\r\n"
		"<!DOCTYPE html><html><head>"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
		"<title>Cheatcoin balance viewer</title>"
		"</head><body>"
		"<p>Enter cheatcoin address to see the balance: "
		"<form action='/cgi-bin/balance.cgi?'><input type=text size=30 name=a></form></p>"
		"</body></html>");
	return 0;
}
