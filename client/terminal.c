#include "terminal.h"
#include <stdlib.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#endif
#include <sys/socket.h>
#include "commands.h"
#include "init.h"
#include "transport.h"
#include "log.h"

#if defined (__APPLE__) || defined (__MACOS__)
#include <string.h>
#endif

#include "../dnet/system.h"

#if defined(_WIN32) || defined(_WIN64)
#define poll WSAPoll
#else
#include <poll.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#define UNIX_SOCK  "unix_sock.dat"
#else
const uint32_t LOCAL_HOST_IP = 0x7f000001; // 127.0.0.1
const uint32_t APPLICATION_DOMAIN_PORT = 7676;
#endif

#include "../utils/utils.h"

int terminal(void)
{
	char *lasts;
	int sock;
	
	if (system_init() != 0) {
		printf("Can't initialize sockets");
	}

	char cmd[XDAG_COMMAND_MAX];
	char cmd2[XDAG_COMMAND_MAX];

	while (1) {
		int ispwd = 0, c = 0;
		printf("%s> ", g_progname); fflush(stdout);
		fgets(cmd, XDAG_COMMAND_MAX, stdin);
		strcpy(cmd2, cmd);
		char *ptr = strtok_r(cmd2, " \t\r\n", &lasts);
		if (!ptr) continue;
		if (!strcmp(ptr, "exit")) break;
		if (!strcmp(ptr, "xfer")) {
			uint32_t pwd[4];
			xdag_user_crypt_action(pwd, 0, 4, 4);
			sprintf(cmd2, "pwd=%08x%08x%08x%08x ", pwd[0], pwd[1], pwd[2], pwd[3]);
			ispwd = 1;
		}
#if !defined(_WIN32) && !defined(_WIN64)
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			printf("Can't open unix domain socket errno:%d.\n", errno);
			continue;
		}
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, UNIX_SOCK);
		if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			printf("Can't connect to unix domain socket errno:%d\n", errno);
			continue;
		}
#else
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			printf("Can't create domain socket errno:%d", WSAGetLastError());
			return 0;
		}
		struct sockaddr_in addrLocal;
		addrLocal.sin_family = AF_INET;
		addrLocal.sin_port = htons(APPLICATION_DOMAIN_PORT);
		addrLocal.sin_addr.s_addr = htonl(LOCAL_HOST_IP);
		if (connect(sock, (struct sockadr*)&addrLocal, sizeof(addrLocal)) == -1) {
			printf("Can't connect to domain socket errno:%d", errno);
			return 0;
		}
#endif
		if (ispwd) {
			write(sock, cmd2, strlen(cmd2));
		}
		write(sock, cmd, strlen(cmd) + 1);
		if (!strcmp(ptr, "terminate")) {
			sleep(1);
			close(sock);
			break;
		}
		while (read(sock, &c, 1) == 1 && c) {
			putchar(c);
		}
		close(sock);
	}

	return 0;
}

void *terminal_thread(void *arg)
{
	int sock;
#if !defined(_WIN32) && !defined(_WIN64)
	struct sockaddr_un addr;
	
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't create unix domain socket errno:%d", errno);
		return 0;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK);
	unlink(UNIX_SOCK);
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		xdag_err("Can't bind unix domain socket errno:%d", errno);
		return 0;
	}
#else	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		xdag_err("Can't create domain socket errno:%d", WSAGetLastError());
		return 0;
	}
	struct sockaddr_in addrLocal;
	addrLocal.sin_family = AF_INET;
	addrLocal.sin_port = htons(APPLICATION_DOMAIN_PORT);
	addrLocal.sin_addr.s_addr = htonl(LOCAL_HOST_IP);
	if (bind(sock, (struct sockadr*)&addrLocal, sizeof(addrLocal)) == -1) {
		xdag_err("Can't bind domain socket errno:%d", errno);
		return 0;
	}
#endif
	if (listen(sock, 100) == -1) {
		xdag_err("Unix domain socket listen errno:%d", errno);
		return 0;
	}
	while (1) {
		char cmd[XDAG_COMMAND_MAX];
		int clientSock, res;
		struct pollfd fds;
		if ((clientSock = accept(sock, NULL, NULL)) == -1) {
			xdag_err("Unix domain socket accept errno:%d", errno);
			break;
		}

		int p = 0;
		fds.fd = clientSock;
		fds.events = POLLIN;
		do {
			if (poll(&fds, 1, 1000) != 1 || !(fds.revents & POLLIN)) {
				res = -1;
				break;
			}
			p += res = read(clientSock, &cmd[p], sizeof(cmd) - p);
		} while (res > 0 && p < sizeof(cmd) && cmd[p - 1] != '\0');

		if (res < 0 || cmd[p - 1] != '\0') {
			close(clientSock);
		} else {
#if !defined(_WIN32) && !defined(_WIN64)
			FILE *fd = fdopen(clientSock, "w");
			if (!fd) {
				xdag_err("Can't fdopen unix domain socket errno:%d", errno);
				break;
			}
#else
			FILE *fd = tmpfile();
			if (!fd) {
				xdag_err("Can't create a temporary file");
				break;
			}
#endif

			res = xdag_command(cmd, fd);

#if !defined(_WIN32) && !defined(_WIN64)
			xdag_close_file(fd);
#else
			rewind(fd);

			char buf[256];
			while (!feof(fd)) {
				const int length = fread(buf, 1, 256, fd);
				write(clientSock, buf, length);
			}
			xdag_close_file(fd);
			close(clientSock);
#endif
			if (res < 0) {
				exit(0);
			}
		}
	}
	return 0;
}
