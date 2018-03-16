//
//  utils.c
//  xdag
//
//  Created by Rui Xie on 3/16/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#include "utils.h"
#if defined (__APPLE__)|| defined (__MACOS__)
#include <unistd.h>
#include <libgen.h>
#define PATH_MAX 4096
#elif _WIN32
#include <shlwapi.h>
#else
#include <libgen.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

static char g_xdag_current_path[4096] = {0};

void xdag_init_path(char *path) {
#if defined (__APPLE__)|| defined (__MACOS__)
    char * n = dirname(path);
    if (*n != '/' && *n != '\\') {
        char buf[PATH_MAX];
        getcwd(buf, PATH_MAX);
        sprintf(g_xdag_current_path, "%s/%s", n, buf);
    } else {
        sprintf(g_xdag_current_path, "%s", n);
    }
    
#elif _WIN32
    char szPath[MAX_PATH];
    char szBuffer[MAX_PATH];
    char * pszFile;
    
    GetModuleFileName(NULL, (LPTCH)szPath, sizeof(szPath) / sizeof(*szPath));
    GetFullPathName((LPTSTR)szPath, sizeof(szBuffer) / sizeof(*szBuffer), (LPTSTR)szBuffer, (LPTSTR*)&pszFile);
    *pszFile = 0;
    
    sprintf(g_xdag_current_path, "%s", szBuffer);
    
#else
    char result[PATH_MAX];
    if (readlink("/proc/self/exe", result, PATH_MAX) > 0) {
        sprintf(g_xdag_current_path, "%s", result);
    } else {
        sprintf(g_xdag_current_path, "%s", "");
    }
#endif
}

FILE* xdag_open_file(const char *path, const char *mode) {
    FILE* f = NULL;
    char abspath[1024];
    sprintf(abspath, "%s/%s", g_xdag_current_path, path);
    f = fopen(abspath, mode);
    return f;
}

void xdag_close_file(FILE *f) {
    fclose(f);
}
