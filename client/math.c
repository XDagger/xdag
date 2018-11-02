/* math, T14.579-T14.618 $DVS:time$ */

#include <stdio.h>
#include <math.h>
#include "init.h"
#include "math.h"
#include "utils/log.h"

// convert xdag_amount_t to long double
inline long double amount2xdags(xdag_amount_t amount)
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

xdag_diff_t xdag_hash_difficulty(xdag_hash_t hash)
{
	xdag_diff_t res = ((xdag_diff_t*)hash)[1];
	xdag_diff_t max = xdag_diff_max;

	xdag_diff_shr32(&res);

#if !defined(_WIN32) && !defined(_WIN64)
	if(!res) {
		xdag_warn("hash_difficulty higher part of hash is equal zero");	
		return max;
	}
#endif
	return xdag_diff_div(max, res);
}

long double xdag_diff2log(xdag_diff_t diff)
{
	long double res = (long double)xdag_diff_to64(diff);
	xdag_diff_shr32(&diff);
	xdag_diff_shr32(&diff);
	if(xdag_diff_to64(diff)) {
		res += ldexpl((long double)xdag_diff_to64(diff), 64);
	}
	return (res > 0 ? logl(res) : 0);
}

long double xdag_log_difficulty2hashrate(long double log_diff)
{
	return ldexpl(expl(log_diff), -58)*(0.65);
}

long double xdag_hashrate(xdag_diff_t *diff)
{
	long double sum = 0;
	for(int i = 0; i < HASHRATE_LAST_MAX_TIME; ++i) {
		sum += xdag_diff2log(diff[i]);
	}
	sum /= HASHRATE_LAST_MAX_TIME;
	return ldexpl(expl(sum), -58); //shown pool and network hashrate seems to be around 35% higher than real, to consider *(0.65) about correction. Deeper study is needed.
}

