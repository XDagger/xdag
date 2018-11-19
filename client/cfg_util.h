#ifndef _CFG_UTIL_H_
#define _CFG_UTIL_H_

typedef int bool;

#ifndef null
#define null ((void *)0)
#endif

#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif

bool isEmpty(const char *str);

char *fgetline(FILE *fp);

char *trim(char *str);


typedef struct
{
    int length;
    char **array;
} SPLIT_STRING;

SPLIT_STRING *split(const char *str, const char *delimiter);

void splitFree(SPLIT_STRING *strings);

#endif
