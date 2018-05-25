//
//  http.h
//  xdag
//
//  Created by Rui Xie on 5/25/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef http_h
#define http_h

#include <stdio.h>

extern size_t http_get(const char* url, uint8_t *buffer);

extern int test_https(void);

#endif /* http_h */
