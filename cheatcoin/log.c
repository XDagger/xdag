/* логирование, T13.670-T13.740 $DVS:time$ */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>
#include "system.h"
#include "log.h"

#define CHEATCOIN_LOG_FILE "cheatcoin.log"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_level = CHEATCOIN_INFO;

int cheatcoin_log(int level, const char *format, ...) {
	static const char lvl[] = "NONEFATACRITINTEERROWARNMESSINFODBUGTRAC";
	char tbuf[64];
	struct tm tm;
	va_list arg;
	struct timeval tv;
	FILE *f;
	int done;
	time_t t;

	if (level < 0 || level > CHEATCOIN_TRACE) level = CHEATCOIN_INTERNAL;
	if (level > log_level) return 0;
	gettimeofday(&tv, 0);
	t = tv.tv_sec;
	localtime_r(&t, &tm);
	strftime(tbuf, 64, "%Y-%m-%d %H:%M:%S", &tm);
	pthread_mutex_lock(&log_mutex);
	f = fopen(CHEATCOIN_LOG_FILE, "a");
	if (!f) { done = -1; goto end; }
	fprintf(f, "%s.%03d [%012lx:%.4s]  ", tbuf, (int)(tv.tv_usec / 1000), (long)pthread_self_ptr(), lvl + 4 * level);

	va_start(arg, format);
	done = vfprintf(f, format, arg);
	va_end(arg);

	fprintf(f, "\n");
	fclose(f);
end:
	pthread_mutex_unlock(&log_mutex);

    return done;
}

extern char *cheatcoin_log_array(const void *arr, unsigned size) {
	static int k = 0;
	static char buf[4][0x1000];
	char *res = &buf[k++ & 3][0];
	unsigned i;
	for (i = 0; i < size; ++i) sprintf(res + 3 * i - !!i, "%s%02x", (i ? ":" : ""), ((uint8_t *)arr)[i]);
	return res;
}

/* устанавливает максимальный уровень ошибки для вывода в лог, возвращает прежний уровень (0 - ничего не выводить, 9 - всё) */
extern int cheatcoin_set_log_level(int level) {
	int level0 = log_level;
	if (level >= 0 && level <= CHEATCOIN_TRACE) log_level = level;
	return level0;
}
