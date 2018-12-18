// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <openssl/rand.h>
#include "log.h"

static void RandFailure()
{
	xdag_fatal("Failed to read randomness, aborting\n");
	abort();
}

static inline int64_t GetPerformanceCounter()
{
	int64_t counter = 0;
#ifdef _WIN32
	QueryPerformanceCounter((LARGE_INTEGER*)&counter);
#else
	timeval t;
	gettimeofday(&t, NULL);
	nCounter = (int64_t)(t.tv_sec * 1000000 + t.tv_usec);
#endif
	return counter;
}

void RandAddSeed()
{
#ifdef _WIN32
	// mix the contents of the screen into the generator.
	RAND_screen();
#endif

	// Seed with CPU performance counter
	int64_t nCounter = GetPerformanceCounter();
	RAND_add(&nCounter, sizeof(nCounter), 1.5);
}

void GetRandBytes(unsigned char* buf, int num)
{
	if(RAND_bytes(buf, num) != 1) {
		RandFailure();
	}
}

uint64_t GetRand(uint64_t max)
{
	if(max == 0) {
		return 0;
	}

	// The range of the random source must be a multiple of the modulus
	// to give every possible output value an equal possibility
	uint64_t range = UINT64_MAX / max * max;
	uint64_t randValue = 0;
	do {
		GetRandBytes((unsigned char*)&randValue, sizeof(randValue));
	} while(randValue >= range);
	return (randValue % max);
}

int GetRandInt(int max)
{
	return GetRand(max);
}
