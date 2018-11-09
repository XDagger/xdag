/* time functions, T14.524-T14.582 $DVS:time$ */

#ifndef XDAG_TIME_H
#define XDAG_TIME_H

#include <stdint.h>
#include "types.h"

/* the maximum period of time for which blocks are requested, not their amounts */
#define REQUEST_BLOCKS_MAX_TIME (1 << 20)

#define HASHRATE_LAST_MAX_TIME  (64 * 4) // numbers of main blocks in about 4H, to calculate the pool and network mean hashrate
#define DEF_TIME_LIMIT          0 // (MAIN_CHAIN_PERIOD / 2)
#define MAIN_CHAIN_PERIOD       (64 << 10)
#define MAIN_TIME(t)            ((t) >> 16)
#define XDAG_TEST_ERA           0x16900000000ll
#define XDAG_MAIN_ERA           0x16940000000ll
#define XDAG_ERA                g_xdag_era
#define MAX_TIME_NMAIN_STALLED  (1 << 10)

#ifdef __cplusplus
extern "C" {
#endif

extern xtime_t g_xdag_era;

// returns a time period index, where a period is 64 seconds long
xtime_t xdag_main_time(void);

// returns the time period index corresponding to the start of the network
xtime_t xdag_start_main_time(void);

// initialize time modeule
int xdag_time_init(void);

// convert xtime_t to string representation
// minimal length of string buffer `buf` should be 60
void xdag_xtime_to_string(xtime_t time, char *buf);

// convert time_t to string representation
// minimal length of string buffer `buf` should be 50
void xdag_time_to_string(time_t time, char* buf);

extern xtime_t xdag_get_xtimestamp(void);

extern uint64_t xdag_get_time_ms(void);
	
#ifdef __cplusplus
};
#endif

#endif
