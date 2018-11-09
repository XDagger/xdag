/* logging, T13.670-T13.895 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include "../system.h"
#include "log.h"
#include "../init.h"
#include "utils.h"

//#define LOG_PRINT // print log to stdout

#define RING_BUFFER_SIZE 2048
#define MAX_POLL_SIZE (RING_BUFFER_SIZE - 4)
#define SEM_LOG_WRITER "/xdaglogwritersem"

typedef unsigned char boolean;
#ifndef FALSE
#   define FALSE (0x0)
#endif
#ifndef TRUE
#   define TRUE (0x1)
#endif

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_level = XDAG_INFO;

static char g_ring_buffer[RING_BUFFER_SIZE] = {0};
static size_t g_write_index = 0;
static size_t g_read_index = 0;
static boolean g_buffer_full = FALSE;

size_t get_used_size(void);
size_t get_free_size(void);
size_t put_log(const char*, size_t);
size_t get_log(char*, size_t);

size_t get_used_size(void)
{
	if(g_write_index > g_read_index) {
		return g_write_index - g_read_index;
	} else if(g_write_index < g_read_index) {
		return g_write_index + RING_BUFFER_SIZE - g_read_index;
	} else {
		return g_buffer_full ? RING_BUFFER_SIZE : 0;
	}
}

size_t get_free_size(void)
{
	return RING_BUFFER_SIZE - get_used_size();
}

size_t put_log(const char* log, size_t size)
{
	size_t freesize = get_free_size();
	if(!freesize) {
		size = 0;
		g_buffer_full = TRUE;
	}

	if(freesize < size) {
		size = freesize;
		g_buffer_full = TRUE;
	}
	
	if(size > 0) {
		if(g_write_index + size < RING_BUFFER_SIZE) {
			memcpy(g_ring_buffer + g_write_index, log, size);
			g_write_index += size;
		} else {
			memcpy(g_ring_buffer + g_write_index, log, RING_BUFFER_SIZE - g_write_index);
			memcpy(g_ring_buffer, log + RING_BUFFER_SIZE - g_write_index, size + g_write_index - RING_BUFFER_SIZE);
			g_write_index += size - RING_BUFFER_SIZE;
		}
	}
	
	return size;
}

size_t get_log(char* log, size_t size)
{
	size_t used = get_used_size();
	if (!used) {
		size = 0;
	}
	
	if(used < size) {
		size = used;
	}
	
	if(size > 0) {
		if(g_read_index + size < RING_BUFFER_SIZE) {
			memcpy(log, (const char*)g_ring_buffer + g_read_index, size);
			g_read_index += size;
		} else {
			memcpy(log, (const char*)g_ring_buffer + g_read_index, RING_BUFFER_SIZE - g_read_index);
			memcpy(log + RING_BUFFER_SIZE - g_read_index, (const char*)g_ring_buffer, size + g_read_index - RING_BUFFER_SIZE);
			g_read_index += size - RING_BUFFER_SIZE;
		}
	}
	
	g_buffer_full = FALSE;
	return size;
}

int xdag_log(const char *logfile, int level, const char *format, ...)
{
	static const char lvl[] = "NONEFATACRITINTEERROWARNMESSINFODBUGTRAC";
	char tbuf[64] = {0}, buf[64] = {0};
	struct tm tm;
	va_list arg;
	struct timeval tv;
	FILE *f;
	int done = 0;
	time_t t;
	
	if (level < 0 || level > XDAG_TRACE) {
		level = XDAG_INTERNAL;
	}
	
	if (level > log_level) {
		return 0;
	}
	
	gettimeofday(&tv, 0);
	t = tv.tv_sec;
	localtime_r(&t, &tm);
	strftime(tbuf, 64, "%Y-%m-%d %H:%M:%S", &tm);
	
	pthread_mutex_lock(&log_mutex);
	sprintf(buf, "%s", logfile);
	
	f = xdag_open_file(buf, "a");
	if (!f) {
		done = -1; goto end;
	}
	
#ifdef LOG_PRINT
	printf("%s.%03d [%012llx:%.4s]  ", tbuf, (int)(tv.tv_usec / 1000), (long long)pthread_self_ptr(), lvl + 4 * level);
#else
	fprintf(f, "%s.%03d [%012llx:%.4s]  ", tbuf, (int)(tv.tv_usec / 1000), (long long)pthread_self_ptr(), lvl + 4 * level);
#endif
	
	va_start(arg, format);
	
	
#ifdef LOG_PRINT
	vprintf(format, arg);
#else
	done = vfprintf(f, format, arg);
#endif

	va_end(arg);

#ifdef LOG_PRINT
	printf("\n");
#else
	fprintf(f, "\n");
#endif
	
	xdag_close_file(f);

 end:
	pthread_mutex_unlock(&log_mutex);

	return done;
}

extern char *xdag_log_array(const void *arr, unsigned size)
{
	static int k = 0;
	static char buf[4][0x1000];
	char *res = &buf[k++ & 3][0];
	unsigned i;

	for (i = 0; i < size; ++i) {
		sprintf(res + 3 * i - !!i, "%s%02x", (i ? ":" : ""), ((uint8_t*)arr)[i]);
	}

	return res;
}

/* sets the maximum error level for output to the log, returns the previous level (0 - do not log anything, 9 - all) */
extern int xdag_set_log_level(int level)
{
	int level0 = log_level;

	if (level >= 0 && level <= XDAG_TRACE) {
		log_level = level;
	}

	return level0;
}


#if !defined(_WIN32) && !defined(_WIN64)
#define __USE_GNU
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#if defined (__MACOS__) || defined (__APPLE__)
#include <execinfo.h>
#include <sys/ucontext.h>
#define RIP_sig(context)     (*((unsigned long*)&(context)->uc_mcontext->__ss.__rip))
#define RSP_sig(context)     (*((unsigned long*)&(context)->uc_mcontext->__ss.__rsp))
#define TRAP_sig(context)    ((context)->uc_mcontext->__es.__trapno)
#define ERR_sig(context)     ((context)->uc_mcontext->__es.__err)
#define EFL_sig(context)     ((context)->uc_mcontext->__ss.__rflags)
#define CR2_sig(context)     ((char *) info->si_addr)
#define RAX_sig(context)     ((context)->uc_mcontext->__ss.__rax)
#define RBX_sig(context)     ((context)->uc_mcontext->__ss.__rbx)
#define RCX_sig(context)     ((context)->uc_mcontext->__ss.__rcx)
#define RDX_sig(context)     ((context)->uc_mcontext->__ss.__rdx)
#define RSI_sig(context)     ((context)->uc_mcontext->__ss.__rsi)
#define RDI_sig(context)     ((context)->uc_mcontext->__ss.__rdi)
#define RBP_sig(context)     ((context)->uc_mcontext->__ss.__rbp)
#define R8_sig(context)      ((context)->uc_mcontext->__ss.__r8)
#define R9_sig(context)      ((context)->uc_mcontext->__ss.__r9)
#define R10_sig(context)     ((context)->uc_mcontext->__ss.__r10)
#define R11_sig(context)     ((context)->uc_mcontext->__ss.__r11)
#define R12_sig(context)     ((context)->uc_mcontext->__ss.__r12)
#define R13_sig(context)     ((context)->uc_mcontext->__ss.__r13)
#define R14_sig(context)     ((context)->uc_mcontext->__ss.__r14)
#define R15_sig(context)     ((context)->uc_mcontext->__ss.__r15)

#define REG_(name) sprintf(buf + strlen(buf), #name "=%llx, ",(unsigned long long)name##_sig(uc))

#elif defined (__linux__)
#include <execinfo.h>
#include <ucontext.h>
#define REG_(name) sprintf(buf + strlen(buf), #name "=%llx, ", (unsigned long long)uc->uc_mcontext.gregs[REG_ ## name])
#endif


static void sigCatch(int signum, siginfo_t *info, void *context)
{

	xdag_fatal("Signal %d delivered", signum);

#if defined (__linux__) || ( defined (__MACOS__) || defined (__APPLE__) )

        static void *callstack[100];
        int frames, i;
        char **strs;

#ifdef __x86_64__
	{
		static char buf[0x100]; *buf = 0;
		ucontext_t *uc = (ucontext_t*)context;
		REG_(RIP); REG_(EFL); REG_(ERR); REG_(CR2);
		xdag_fatal("%s", buf); *buf = 0;
		REG_(RAX); REG_(RBX); REG_(RCX); REG_(RDX); REG_(RSI); REG_(RDI); REG_(RBP); REG_(RSP);
		xdag_fatal("%s", buf); *buf = 0;
		REG_(R8); REG_(R9); REG_(R10); REG_(R11); REG_(R12); REG_(R13); REG_(R14); REG_(R15);
		xdag_fatal("%s", buf);
	}
#endif
	frames = backtrace(callstack, 100);
	strs = backtrace_symbols(callstack, frames);

	for (i = 0; i < frames; ++i) {
		xdag_fatal("%s", strs[i]);
	}
#endif
	signal(signum, SIG_DFL);
	kill(getpid(), signum);

	exit(-1);
}

int xdag_log_init(void)
{
	int i;
	struct sigaction sa;

	sa.sa_sigaction = sigCatch;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_SIGINFO;

	for (i = 1; i < 32; ++i) {
		if (i != SIGURG && i != SIGCHLD && i != SIGCONT && i != SIGPIPE && i != SIGINT && i != SIGTERM && i != SIGWINCH && i != SIGHUP) {
			sigaction(i, &sa, 0);
		}
	}

	xdag_mess("Initializing log system...");
	
	return 0;
}

#else

int xdag_log_init(void)
{
	return 0;
}

#endif
