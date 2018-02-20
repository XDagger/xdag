/* addresses, T13.692-T13.720 $DVS:time$ */

#include <stdint.h>
#include <string.h>
#include "address.h"

static const uint8_t bits2mime[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8_t mime2bits[256];

// intializes the address module
int xdag_address_init(void)
{
	int i;

	memset(mime2bits, 0xFF, 256);

	for (i = 0; i < 64; ++i) {
		mime2bits[bits2mime[i]] = i;
	}

	return 0;
}

// converts address to hash
int xdag_address2hash(const char *address, xdag_hash_t hash)
{
	uint8_t *fld = (uint8_t*)hash;
	int i, c, d, e, n;

	for (e = n = i = 0; i < 32; ++i) {
		do {
			if (!(c = (uint8_t)*address++))
				return -1;
			d = mime2bits[c];
		} while (d & 0xC0);
		e <<= 6;
		e |= d;
		n += 6;

		if (n >= 8) {
			n -= 8;
			*fld++ = e >> n;
		}
	}

	for (i = 0; i < 8; ++i) {
		*fld++ = 0;
	}

	return 0;
}

// converts hash to address
const char *xdag_hash2address(const xdag_hash_t hash)
{
	static char bufs[4][33];
	static int k = 0;
	char *buf = &bufs[k++ & 3][0], *ptr = buf;
	int i, c, d;
	const uint8_t *fld = (const uint8_t*)hash;

	for (i = c = d = 0; i < 32; ++i) {
		if (d < 6) {
			d += 8;
			c <<= 8;
			c |= *fld++;
		}
		d -= 6;
		*ptr++ = bits2mime[c >> d & 0x3F];
	}

	*ptr = 0;
	
	return buf;
}
