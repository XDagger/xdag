/* memory, T13.816-T13.889 $DVS:time$ */

#ifndef XDAG_MEMORY_H
#define XDAG_MEMORY_H

extern int xdag_mem_init(size_t size);

extern void *xdag_malloc(size_t size);

extern void xdag_free(void *mem);

extern void xdag_mem_finish(void);

extern int xdag_free_all(void);

extern char** xdagCreateStringArray(int count, int stringLen);
extern void xdagFreeStringArray(char** stringArray, int count);

#endif
