/* time functions, T14.524-T14.582 $DVS:time$ */

#include "time.h"
#include "utils/utils.h"

xdag_time_t g_time_limit = DEF_TIME_LIMIT, g_xdag_era = XDAG_MAIN_ERA;
extern int g_xdag_testnet;

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

int xdag_time_init(void)
{
	if (g_xdag_testnet) {
		g_xdag_era = XDAG_TEST_ERA;
	}
	
	return 1;
}
