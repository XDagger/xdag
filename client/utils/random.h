// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

/* Seed OpenSSL PRNG with additional entropy data */
void RandAddSeed(void);

/**
 * Functions to gather random data via the OpenSSL PRNG
 */
void GetRandBytes(void* buf, int num);
uint64_t GetRand(uint64_t nMax);
int GetRandInt(int nMax);

#endif // RANDOM_H
