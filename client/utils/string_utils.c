#include <stdlib.h>
#include <string.h>
#include "string_utils.h"

int string_is_empty(const char *s)
{
	return s == NULL || *s == 0;
}

char *string_trim(char *str)
{
	if(str != NULL) {
		int len = (int)strlen(str);
		char *start = str;
		char *end = str + len;
		while(start < end && *start <= ' ') {
			start++;
		}
		while(end > start && *(end - 1) <= ' ') {
			end--;
		}
		if(end - start != len) {
			strncpy(str, start, end - start);
			str[end - start] = '\0';
		}
	}
	return str;
}