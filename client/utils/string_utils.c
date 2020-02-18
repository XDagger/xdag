#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string_utils.h"

int string_is_empty(const char *s)
{
	return s == NULL || *s == 0;
}

static char* rtrim(char *str)
{
    if (str == NULL || *str == '\0')
    {
        return str;
    }
 
    char *p = str + strlen(str) - 1;
    while (p >= str  && isspace(*p))
    {
        *p = '\0';
        --p;
    }
 
    return str;
}

static char* ltrim(char *str)
{
    if (str == NULL || *str == '\0')
    {
        return str;
    }
 
    int len = 0;
    char *p = str;
    while (*p != '\0' && isspace(*p))
    {
        ++p;
        ++len;
    }
 
    memmove(str, p, strlen(str) - len + 1);
 
    return str;
}

char* string_trim(char *str)
{
    str = rtrim(str);
    str = ltrim(str);
    
    return str;
}
