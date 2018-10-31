#include <stdio.h>
#include <intrin.h>

#include "stdatomic.h"

uint_least64_t atomic_exchange(atomic_uint_least64_t *object, uint_least64_t desired)
{
	return _InterlockedExchange64((volatile long long*)object, desired);
}

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
