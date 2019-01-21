//
//  uuid.c
//  xdag
//
//  Created by Rui Xie on 12/26/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "uuid.h"
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#endif


static const char *template = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
static const char *chars = "0123456789abcdef";

/* http://xorshift.di.unimi.it/xorshift128plus.c */
static uint64_t xorshift128plus(uint64_t *s) {
	uint64_t s1 = s[0];
	uint64_t s0 = s[1];
	s[0] = s0;
	s1 ^= s1 << 23;
	s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
	return s[1] + s0;
}

int uuid(char *buf)
{
	uint64_t seed[2];

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
	size_t res;
	FILE *fp = fopen("/dev/urandom", "rb");
	if (!fp) {
		printf("uuid failed.");
		return -1;
	}
	res = fread(seed, 1, sizeof(seed), fp);
	fclose(fp);
	if ( res != sizeof(seed) ) {
		printf("uuid failed.");
		return -1;
	}

#elif defined(_WIN32)
	int res;
	HCRYPTPROV hCryptProv;
	res = CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	if (!res) {
		printf("uuid failed.");
		return -1;
	}
	res = CryptGenRandom(hCryptProv, (DWORD) sizeof(seed), (PBYTE) seed);
	CryptReleaseContext(hCryptProv, 0);
	if (!res) {
		printf("uuid failed.");
		return -1;
	}
#endif

	union { unsigned char b[16]; uint64_t word[2]; } s;
	const char *p = template;
	int i, n;

	s.word[0] = xorshift128plus(seed);
	s.word[1] = xorshift128plus(seed);

	i = 0;
	while (*p) {
		n = s.b[i >> 1];
		n = (i & 1) ? (n >> 4) : (n & 0xf);
		switch (*p) {
			case 'x': {
				*buf = chars[n];
				i++;
				break;
			}

			case 'y': {
				*buf = chars[(n & 0x3) + 8];
				i++;
				break;
			}
			default: {
				*buf = *p;
				break;
			}
		}
		buf++;
		p++;
	}
	*buf = '\0';

	return 0;
}
