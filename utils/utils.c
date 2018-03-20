//
//  utils.c
//  xdag
//
//  Created by Rui Xie on 3/16/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "utils.h"
#include <sys/stat.h>
#if defined (__APPLE__)|| defined (__MACOS__)
#include <unistd.h>
#include <libgen.h>
#define PATH_MAX 4096
#elif defined (_WIN32)
#include <direct.h>
#include <shlwapi.h>
#else
#include <libgen.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

static char g_xdag_current_path[4096] = {0};

void xdag_init_path(char *path) 
{
#if defined (__APPLE__)|| defined (__MACOS__)
    char * n = dirname(path);
    if (*n != '/' && *n != '\\') {
        char buf[PATH_MAX];
        getcwd(buf, PATH_MAX);
        sprintf(g_xdag_current_path, "%s/%s", buf, n);
    } else {
        sprintf(g_xdag_current_path, "%s", n);
    }

#elif _WIN32
    char szPath[MAX_PATH];
    char szBuffer[MAX_PATH];
    char *pszFile;
    
    GetModuleFileName(NULL, (LPTCH)szPath, sizeof(szPath) / sizeof(*szPath));
    GetFullPathName((LPTSTR)szPath, sizeof(szBuffer) / sizeof(*szBuffer), (LPTSTR)szBuffer, (LPTSTR*)&pszFile);
    *pszFile = 0;
    
    strcpy(g_xdag_current_path, szBuffer);
#else
    char result[PATH_MAX];
    if (readlink("/proc/self/exe", result, PATH_MAX) > 0) {
		strcpy(g_xdag_current_path, result);
    } else {
		g_xdag_current_path[0] = 0;
    }
#endif

	const int pathLen = strlen(g_xdag_current_path);
	if (pathLen == 0 || g_xdag_current_path[pathLen - 1] != *DELIMITER) {
		g_xdag_current_path[pathLen] = *DELIMITER;
		g_xdag_current_path[pathLen + 1] = 0;
	}
}

FILE* xdag_open_file(const char *path, const char *mode) 
{
    char abspath[1024];
    sprintf(abspath, "%s%s", g_xdag_current_path, path);
	FILE* f = fopen(abspath, mode);
    return f;
}

void xdag_close_file(FILE *f) 
{
    fclose(f);
}

int xdag_file_exists(const char *path)
{
	char abspath[1024];
	sprintf(abspath, "%s%s", g_xdag_current_path, path);
	struct stat st;
	return !stat(abspath, &st);
}

int xdag_mkdir(const char *path)
{
	char abspath[1024];
	sprintf(abspath, "%s%s", g_xdag_current_path, path);

#if defined(_WIN32)
	return _mkdir(abspath);
#else 
	return mkdir(abspath, 0770);
#endif	
}
