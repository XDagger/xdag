/* math, T14.579-T14.579 $DVS:time$ */

#ifndef XDAG_MATH_H
#define XDAG_MATH_H

#include <stdint.h>
#include "types.h"

#define xdag_amount2xdag(amount) ((unsigned)((amount) >> 32))
#define xdag_amount2cheato(amount) ((unsigned)(((uint64_t)(unsigned)(amount) * 1000000000) >> 32))

#ifdef __cplusplus
extern "C" {
#endif

// convert cheato to xdag
extern long double amount2xdags(xdag_amount_t amount);

// convert xdag to cheato
extern xdag_amount_t xdags2amount(const char *str);
	
#ifdef __cplusplus
};
#endif

#endif
