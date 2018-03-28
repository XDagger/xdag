#include "utils.h"


int conversion_toInt(long double a, int* b) {
	if (a > (long double)INT_MAX || a < (long double)-INT_MAX)		
		return -1;
	*b = (int)a;
	return 0;
}

int conversion_toLong(long double a, long* b) {
	if (a > (long double)LONG_MAX || a < (long double)-LONG_MAX)
		return -1;
	*b = (long)a;
	return 0;
}
