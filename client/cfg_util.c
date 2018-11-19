#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cfg_util.h"

bool isEmpty(const char *s)
{
    return (s == null) || (strlen(s) == 0);
}

char *fgetline(FILE *fp)
{
    char buf[128];
    char c;
    char *line = null;
    int line_len = 0, buf_len = 0;

    while(!feof(fp))
    {
        memset(buf, 0, sizeof(buf));
        fgets(buf, sizeof(buf), fp);
        buf_len = strlen(buf);
        line = (char *)realloc(line, line_len+buf_len+1);
        strcpy(line+line_len, buf);
        line_len += buf_len;

        /**
         * fgets() reads '\n' and set '\0' for last character.
         * So if line length < buf length, "c" should be '\0'. If line length == buf length, "c" should be '\n'.
         * Else "c" should be one character, then we should read more.
         */
        c = buf[sizeof(buf)-2];
        if(c == '\0' || c == '\n')
        {
            break;
        }
    }
    return line;
}

char *trim(char *str)
{
    if(str != null)
    {
        int len = (int)strlen(str);
        char *start = str;
        char *end = str + len;
        while(start < end && *start <= ' ')
        {
            start++;
        }
        while(end > start && *(end-1) <= ' ')
        {
            end--;
        }
        if(end - start != len)
        {
            strncpy(str, start, end-start);
            str[end-start] = '\0';
        }
    }
    return str;
}

SPLIT_STRING *split(const char *str, const char *delimiter)
{
    if(!isEmpty(str) && !isEmpty(delimiter))
    {
        int str_len = (int)strlen(str);
        int delimiter_len = (int)strlen(delimiter);
        SPLIT_STRING *strings = (SPLIT_STRING *)malloc(sizeof(SPLIT_STRING));
        char *start, *end;
        strings->length = 0;
        strings->array = null;
        start = (char *)str;
        do
        {
            end = strstr(start, delimiter);
            if(end == null)
            {
                end = (char *)str + str_len;
            }
            strings->length++;
            strings->array = (char **)realloc(strings->array, strings->length*sizeof(char *));
            strings->array[strings->length-1] = (char *)malloc(end-start+1);
            memcpy(strings->array[strings->length-1], start, end-start);
            strings->array[strings->length-1][end-start] = '\0';

            start = end + delimiter_len;
        }
        while(start < str + str_len);

        return strings;
    }

    return null;
}

void splitFree(SPLIT_STRING *strings)
{
    if(strings != null)
    {
        int i;
        for(i=0; i<strings->length; i++)
        {
            if(strings->array[i] != null)
            {
                free(strings->array[i]);
            }
        }
        if(strings->array != null)
        {
            free(strings->array);
        }
        free(strings);
    }
}
