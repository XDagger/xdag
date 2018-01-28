/* блок-эксплорер */
#define VERSION	"XDAG block explorer T13.856-T13.857" /* $DVS:time$ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define UNIX_SOCK	"/home/ec2-user/.cheatcoin1/unix_sock.dat"

int main(void) {
	char *var, cmd[256], *p, *q, buf[0x10000];
	int c = 0, s, i, j, res;
	struct sockaddr_un addr;
	printf("Content-Type: text/html; charset=utf-8\r\n\r\n"
		"<!DOCTYPE html><html><head>"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
		"<title>XDAG block explorer</title>"
		"</head><body>"
		"<form action='/cgi-bin/block.cgi?'>Enter XDAG address or block hash to explore the block: "
		"<input type=text size=100 name=a></form></p><hr><pre>");
	var = getenv("REQUEST_METHOD");
	if (!var || strcmp(var, "GET")) goto page;
	var = getenv("QUERY_STRING");
	if (!var || var[0] != 'a' || var[1] != '=' || !var[2] || isspace((unsigned char)var[2])) goto page;
	snprintf(cmd, 256, "block %s", var + 2);
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
	if( connect(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) goto end;
	write(s, cmd, strlen(cmd) + 1);
	i = 0;
	while ((res = read(s, buf, 0x10000)) > 0) {
		for (j = 0; j < res; ++j) {
			c = (uint8_t)buf[j];
			if (!c) goto end;
			cmd[i++] = c;
			cmd[i] = 0;
			if (c == '\n') {
				if (i > 44 && cmd[10] == ':' && cmd[44] == ' ')
					printf("%.12s<a href=/cgi-bin/block.cgi?a=%.32s>%.32s</a>%s", cmd, cmd+12, cmd+12, cmd+44);
				else
					printf("%s", cmd);
				i = 0;
			}
		}
	}
end:
	close(s);
page:
	printf("</pre><hr><p>%s</p></body></html>", VERSION);
	return 0;
}
