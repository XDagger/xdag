/* dnet: main file; T11.231-T13.789; $DVS:time$ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#endif
#include "dnet_crypt.h"
#include "dnet_database.h"
#include "dnet_history.h"
#include "dnet_connection.h"
#include "dnet_threads.h"
#include "dnet_log.h"
#include "dnet_command.h"
#include "dnet_main.h"

#if defined (__MACOS__) || defined (__APPLE__)
#define SIGPOLL SIGIO
#endif

//#define NO_DNET_FORK

extern int getdtablesize(void);

#ifdef __LDuS__
#include <ldus/system/kernel.h>
static void catcher(int signum) {
	ldus_block_signal(0, signum);	/* заблокировать его, чтобы самому повторно не получить */
	ldus_kill_task(0, signum);	/* передать сигнал дальше */
}
#endif

static void daemonize(void) {
#if !defined(_WIN32) && !defined(_WIN64) && !defined(QDNET) && !defined(NO_DNET_FORK)
	int i;
#ifndef __LDuS__
    if (getppid() == 1) exit(0); /* already a daemon */
    i = fork();
    if (i < 0) exit(1); /* fork error */
    if (i > 0) exit(0); /* parent exits */

    /* child (daemon) continues */
    setsid(); /* obtain a new process group */
    for (i = getdtablesize(); i >= 0; --i) close(i); /* close all descriptors */
    i = open("/dev/null", O_RDWR); dup(i); dup(i); /* handle standard I/O */

    /* first instance continues */
#if 0
    signal(SIGCHLD, SIG_IGN); /* ignore child */
#endif
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
#else
	/* перехват всех сигналов */
	for (i = 1; i <= INT_SIG_END - INT_SIG; ++i) if (i != SIGCONT)
		signal(i, &catcher);
#endif
#endif
}

static void angelize(void) {
#if !defined(__LDuS__) && !defined(QDNET) && !defined(_WIN32) && !defined(_WIN64) && !defined(NO_DNET_FORK)
    int stat;
    pid_t childpid;
	while ((childpid = fork())) {
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		if (childpid > 0) while (waitpid(childpid, &stat, 0) == -1) {
            if (errno != EINTR) {
                abort();
            }
        }
        if (stat >= 0 && stat <= 5) {
            exit(stat);
        }
        sleep(10);
    }
#endif
}

int dnet_init(int argc, char **argv) {
	char command[DNET_COMMAND_MAX];
    struct dnet_output out;
    struct dnet_thread *thread;
    int i = 0, err = 0, res, is_daemon = 0, is_server = 0;
    const char *mess = 0;

    if (system_init() || dnet_threads_init() || dnet_hosts_init()) {
		err = 4; mess = "initializing error"; goto end;
    }

    for (i = 1; i < argc + 2; ++i) {
		if (i == 1) {
#if !defined(_WIN32) && !defined(_WIN64)
			if (i < argc && !strcmp(argv[i], "-d")) is_daemon = 1;
#endif
			printf("%s %s%s.\n", argv[0], DNET_VERSION, (is_daemon ? ", running as daemon" : ""));
			if ((err = dnet_crypt_init(DNET_VERSION))) {
				sleep(3); printf("Password incorrect.\n");
				return err;
			}
		work:
			if (is_daemon) daemonize();
			angelize();
			dnet_log_printf("%s %s%s.\n", argv[0], DNET_VERSION, (is_daemon ? ", running as daemon" : ""));
			if (is_daemon) continue;
		}
		if (i < argc && !strcmp(argv[i], "-s")) {
			is_server = 1;
			continue;
		}
		if (i < argc - 1 && !strcmp(argv[i], "-c")) {
			i++;
			continue;
		}

		thread = malloc(sizeof(struct dnet_thread));
		if (!thread) { err = 2; goto end; }
		if (i < argc) {
			thread->arg = argv[i];
			thread->conn.socket = -1;
			thread->type = (is_server ? DNET_THREAD_SERVER : DNET_THREAD_CLIENT);
		} else if (i == argc) {
			thread->type = DNET_THREAD_WATCHDOG;
		} else {
			thread->type = DNET_THREAD_COLLECTOR;
		}
		res = dnet_thread_create(thread);
		if (res) { err = 3; goto end; }

		is_server = 0;
    }

    out.f = stdout;
    i = 1;
    while (1) {
		while (i < argc && strcmp(argv[i], "-c")) i++;
		if (i + 1 < argc) {
			if (!strcmp(argv[++i], "repeat")) {
				i = 1;
				continue;
			}
			strcpy(command, argv[i++]);
		} else if (is_daemon) {
			return 0;

		} else {
            return 0;
		}
    	if (dnet_command(command, &out) < 0) break;
    }

end:
    if (err) {
        if (!mess) mess = strerror(errno);
		if (!is_daemon) printf("%s: error %X: %s.\n", argv[0], err, mess);
        dnet_log_printf("%s: error %X: %s.\n", argv[0], err, mess);
        if (err < 0) goto work;
    }
    exit(err);
}
