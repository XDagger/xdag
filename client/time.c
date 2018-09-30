/* time functions, T14.524-T14.524 $DVS:time$ */

#include "time.h"
#include "utils/utils.h"

xdag_time_t xdag_era = XDAG_MAIN_ERA;

// returns a time period index, where a period is 64 seconds long
xdag_time_t xdag_main_time(void)
{
	return MAIN_TIME(get_timestamp());
}

// returns the time period index corresponding to the start of the network
xdag_time_t xdag_start_main_time(void)
{
	return MAIN_TIME(XDAG_ERA);
}
