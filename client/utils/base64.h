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



#ifdef __cplusplus
extern "C" {
#endif
	extern int base64_encode(const uint8_t *in, size_t inlen, char **out, size_t *outlen);
	extern int base64_decode(const char *in, size_t inlen, uint8_t **out, size_t *outlen);
#ifdef __cplusplus
};
#endif

#endif /* base64_h */
