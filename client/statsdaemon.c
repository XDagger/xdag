#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#if defined (_MACOS) || defined (_APPLE)
#define SIGPOLL SIGIO
#endif

#define CHEATCOIN_COMMAND_MAX	0x1000
#define UNIX_SOCK				"unix_sock.dat"
#define STATS_TXT				"/home/ec2-user/cheat/stats.txt"

static void daemonize(void) {
	int i;
	if (getppid() == 1) exit(0); /* already a daemon */
	i = fork();
	if (i < 0) exit(1); /* fork error */
	if (i > 0) exit(0); /* parent exits */

	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i = getdtablesize(); i >= 0; --i) close(i); /* close all descriptors */
	i = open("/dev/null", O_RDWR); dup(i); dup(i); /* handle standard I/O */

	/* first instance continues */
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGPOLL, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGVTALRM, SIG_IGN);
	signal(SIGPROF, SIG_IGN);
}

int main(void) {
	const char *cmd = "stats";
	struct sockaddr_un addr;
	daemonize();
	while(1) {
		int c = 0, s;
		FILE *f;
		if( (s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) { printf("Can't open unix domain socket errno:%d.\n",errno); continue; }
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, UNIX_SOCK);
		if( connect(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) { printf("Can't connect to unix domain socket errno:%d\n",errno); continue; }
		write(s, cmd, strlen(cmd) + 1);
		if ((f = fopen(STATS_TXT, "w"))) {
			while (read(s, &c, 1) == 1 && c) fputc(c, f);
			fclose(f);
		}
		close(s);
		sleep(10);
	}
	return 0;
}
