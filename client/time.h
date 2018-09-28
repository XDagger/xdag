/* time functions, T14.524-T14.524 $DVS:time$ */

#ifndef XDAG_TIME_H
#define XDAG_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "types.h"

#define MAIN_TIME(t)            ((t) >> 16)
#define XDAG_TEST_ERA           0x16900000000ll
#define XDAG_MAIN_ERA           0x16940000000ll
#define XDAG_ERA                xdag_era

// returns a time period index, where a period is 64 seconds long
xdag_time_t xdag_main_time(void);

// returns the time period index corresponding to the start of the network
xdag_time_t xdag_start_main_time(void);
	
#ifdef __cplusplus
};
#endif

#endif
