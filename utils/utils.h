//
//  utils.h
//  xdag
//
//  Created by Rui Xie on 3/16/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef XDAG_UTILS_HEADER_H
#define XDAG_UTILS_HEADER_H

#include <stdio.h>
#include <string.h>

extern void xdag_init_path(char *base);
extern FILE* xdag_open_file(const char *path, const char *mode);
extern void xdag_close_file(FILE *f);

#endif /* utils_h */
