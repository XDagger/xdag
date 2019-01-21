//
//  uuid.h
//  xdag
//
//  Created by Rui Xie on 12/26/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef uuid_h
#define uuid_h

#include <stdio.h>

#define UUID4_LEN 37

#ifdef __cplusplus
extern "C" {
#endif
	extern int uuid(char *buf);
#ifdef __cplusplus
};
#endif

#endif /* uuid_h */
