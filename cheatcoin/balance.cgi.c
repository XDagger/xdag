#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define UNIX_SOCK	"/home/ec2-user/.cheatcoin1/unix_sock.dat"

int main(void) {
	char *var, cmd[256], *p, *q;
	int c = 0, s;
	struct sockaddr_un addr;
	var = getenv("REQUEST_METHOD");
	if (!var || strcmp(var, "GET")) goto page;
	var = getenv("QUERY_STRING");
	if (!var || var[0] != 'a' || var[1] != '=' || !var[2] || isspace((unsigned char)var[2])) goto page;
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
	if( (s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) goto page;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK);
	if( connect(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) goto page;
	write(s, cmd, strlen(cmd) + 1);
	printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");
	while (read(s, &c, 1) == 1 && c) putchar(c);
	close(s);
	return 0;
page:
	printf("Content-Type: text/html; charset=utf-8\r\n\r\n"
		"<!DOCTYPE html><html><head>"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
		"<title>XDAG balance viewer</title>"
		"</head><body>"
		"<p>Enter XDAG address to see the balance: "
		"<form action='/cgi-bin/balance.cgi?'><input type=text size=40 name=a></form></p>"
		"</body></html>");
	return 0;
}
