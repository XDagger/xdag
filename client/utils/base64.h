//
//  base64.h
//  xdag
//
//  Created by Rui Xie on 11/16/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef base64_h
#define base64_h

#include <stdio.h>
#include <stddef.h>

/* This uses that the expression (n+(k-1))/k means the smallest
 integer >= n/k, i.e., the ceiling of n/k.  */
#define BASE64_LENGTH(inlen) ((((inlen) + 2) / 3) * 4)

#ifdef __cplusplus
extern "C" {
#endif
	/* encode data to base64 string, out should be freed manually */
	extern int base64_encode(const uint8_t *in, size_t inlen, char **out, size_t *outlen);

	/* decode base64 string to uint8_t array, out should be freed manually */
	extern int base64_decode(const char *in, size_t inlen, uint8_t **out, size_t *outlen);
#ifdef __cplusplus
};
#endif

#endif /* base64_h */
