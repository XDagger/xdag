#include <stdio.h>
#include <intrin.h>

#include "stdatomic.h"

int atomic_compare_exchange_strong(atomic_int *ptr, int *expected, int desired)
{
	int comparand = *expected;
	int dstValue = _InterlockedCompareExchange((volatile long*)ptr, desired, comparand);
	int success = dstValue == comparand;
	if(!success) {
		*expected = dstValue;
	}

	return success;
}
