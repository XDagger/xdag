#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define CHEATCOIN_COMMAND_MAX	0x1000
#define FIFO_IN					"fifo_cmd.dat"
#define FIFO_OUT				"fifo_res.dat"
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
	int fd;
	daemonize();
	while(1) {
		int c = 0;
		FILE *f;
		fd = open(FIFO_IN, O_WRONLY);
		if (fd >= 0) {
			write(fd, cmd, strlen(cmd) + 1);
			close(fd);
			fd = open(FIFO_OUT, O_RDONLY);
			if (fd >= 0 && (f = fopen(STATS_TXT, "w"))) {
				while (read(fd, &c, 1) == 1 && c) fputc(c, f);
				close(fd);
				fclose(f);
			}
		}
		sleep(10);
	}
	return 0;
}
