/* math, T14.579-T14.618 $DVS:time$ */

#ifndef XDAG_MATH_H
#define XDAG_MATH_H

#include <stdint.h>
#include "types.h"
#include "system.h"
#include "hash.h"

#define xdag_amount2xdag(amount) ((unsigned)((amount) >> 32))
#define xdag_amount2cheato(amount) ((unsigned)(((uint64_t)(unsigned)(amount) * 1000000000) >> 32))

#ifdef __cplusplus
extern "C" {
#endif

// convert cheato to xdag
extern long double amount2xdags(xdag_amount_t);

// convert xdag to cheato
extern xdag_amount_t xdags2amount(const char*);

// calculate difficulty from hash
xdag_diff_t xdag_hash_difficulty(xdag_hash_t);

// convert diff to logarithm representation
long double xdag_diff2log(xdag_diff_t);

// convert difficulty represented as logarithm in hashrate
long double xdag_log_difficulty2hashrate(long double);

// calculates hashrate from last HASHRATE_LAST_MAX_TIME recorded difficulties
// diff is actually a pointer to a xdag_diff_t array with HASHRATE_LAST_MAX_TIME elements
long double xdag_hashrate(xdag_diff_t *diff);
	
#ifdef __cplusplus
};
#endif

#endif
