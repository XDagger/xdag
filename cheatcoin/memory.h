/* работа с памятью, T13.816-T13.816 $DVS:time$ */

#ifndef XDAG_MEMORY_H
#define XDAG_MEMORY_H

#include <stddef.h>

extern int xdag_mem_init(size_t size);

extern void *xdag_malloc(size_t size);

extern void xdag_free(void *mem);

extern void xdag_mem_finish(void);

#endif
