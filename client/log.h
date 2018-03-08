/* logging, T13.670-T13.788 $DVS:time$ */

#ifndef XDAG_LOG_H
#define XDAG_LOG_H

enum xdag_debug_levels
{
    XDAG_NOERROR,
    XDAG_FATAL,
    XDAG_CRITICAL,
    XDAG_INTERNAL,
    XDAG_ERROR,
    XDAG_WARNING,
    XDAG_MESSAGE,
    XDAG_INFO,
    XDAG_DEBUG,
    XDAG_TRACE,
};

extern int xdag_log(int level, const char *format, ...);

extern char *xdag_log_array(const void *arr, unsigned size);

extern int xdag_log_init(void);

#define xdag_log_hash(hash) xdag_log_array(hash, sizeof(xdag_hash_t))

// sets the maximum error level for output to the log, returns the previous level (0 - do not log anything, 9 - all)
extern int xdag_set_log_level(int level);

#define xdag_fatal(...) xdag_log(XDAG_FATAL   , __VA_ARGS__)
#define xdag_crit(...)  xdag_log(XDAG_CRITICAL, __VA_ARGS__)
#define xdag_err(...)   xdag_log(XDAG_ERROR   , __VA_ARGS__)
#define xdag_warn(...)  xdag_log(XDAG_WARNING , __VA_ARGS__)
#define xdag_mess(...)  xdag_log(XDAG_MESSAGE , __VA_ARGS__)
#define xdag_info(...)  xdag_log(XDAG_INFO    , __VA_ARGS__)
#ifndef NDEBUG
#define xdag_debug(...) xdag_log(XDAG_DEBUG   , __VA_ARGS__)
#else
#define xdag_debug(...)
#endif

#endif
