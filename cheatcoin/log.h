/* логирование, T13.670-T13.788 $DVS:time$ */

#ifndef CHEATCOIN_LOG_H
#define CHEATCOIN_LOG_H

enum cheatcoin_debug_levels {
	CHEATCOIN_NOERROR,
	CHEATCOIN_FATAL,
	CHEATCOIN_CRITICAL,
	CHEATCOIN_INTERNAL,
	CHEATCOIN_ERROR,
	CHEATCOIN_WARNING,
	CHEATCOIN_MESSAGE,
	CHEATCOIN_INFO,
	CHEATCOIN_DEBUG,
	CHEATCOIN_TRACE,
};

extern int cheatcoin_log(int level, const char *format, ...);

extern char *cheatcoin_log_array(const void *arr, unsigned size);

extern int cheatcoin_log_init(void);

#define cheatcoin_log_hash(hash) cheatcoin_log_array(hash, sizeof(cheatcoin_hash_t))

/* устанавливает максимальный уровень ошибки для вывода в лог, возвращает прежний уровень (0 - ничего не выводить, 9 - всё) */
extern int cheatcoin_set_log_level(int level);

#define cheatcoin_fatal(...) cheatcoin_log(CHEATCOIN_FATAL   , __VA_ARGS__)
#define cheatcoin_crit(...)  cheatcoin_log(CHEATCOIN_CRITICAL, __VA_ARGS__)
#define cheatcoin_err(...)   cheatcoin_log(CHEATCOIN_ERROR   , __VA_ARGS__)
#define cheatcoin_warn(...)  cheatcoin_log(CHEATCOIN_WARNING , __VA_ARGS__)
#define cheatcoin_mess(...)  cheatcoin_log(CHEATCOIN_MESSAGE , __VA_ARGS__)
#define cheatcoin_info(...)  cheatcoin_log(CHEATCOIN_INFO    , __VA_ARGS__)
#ifndef NDEBUG
#define cheatcoin_debug(...) cheatcoin_log(CHEATCOIN_DEBUG   , __VA_ARGS__)
#else
#define cheatcoin_debug(...)
#endif

#endif
