/* math, T14.579-T14.579 $DVS:time$ */

#include <stdio.h>
#include <math.h>
#include "math.h"

// convert xdag_amount_t to long double
long double amount2xdags(xdag_amount_t amount)
{
	return xdag_amount2xdag(amount) + (long double)xdag_amount2cheato(amount) / 1000000000;
}

// convert xdag to cheato
xdag_amount_t xdags2amount(const char *str)
{
	long double sum;
	if(sscanf(str, "%Lf", &sum) != 1 || sum <= 0) {
		return 0;
	}
	long double flr = floorl(sum);
	xdag_amount_t res = (xdag_amount_t)flr << 32;
	sum -= flr;
	sum = ldexpl(sum, 32);
	flr = ceill(sum);
	return res + (xdag_amount_t)flr;
}
