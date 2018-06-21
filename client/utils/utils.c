//
//  utils.c
//  xdag
//
//  Copyright © 2018 xdag contributors.
//

#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#if defined (__APPLE__)|| defined (__MACOS__)
#include <libgen.h>
#define PATH_MAX 4096
#elif defined (_WIN32)
#include <direct.h>
#include <shlwapi.h>
#else
#include <libgen.h>
#include <linux/limits.h>
#endif
#include "../uthash/utlist.h"
#include "log.h"
#include "../system.h"
#include "math.h"

static pthread_mutex_t g_detect_mutex = PTHREAD_MUTEX_INITIALIZER;

struct mutex_thread_element {
	uint64_t tid;
	pthread_mutex_t *mutex_ptr;
	char mutex_name[128];
	struct mutex_thread_element *next;
};

struct mutex_thread_vertex {
	uint64_t tid;
	int in_degrees;
	struct mutex_thread_element *depends;
	struct mutex_thread_vertex *next;
};

static struct mutex_thread_element *try_list_mutex_thread = NULL;
static struct mutex_thread_element *list_mutex_thread = NULL;

void apply_lock_before(uint64_t tid, pthread_mutex_t *mutex_ptr, const char *name)
{
	pthread_mutex_lock(&g_detect_mutex);
	struct mutex_thread_element *elem = (struct mutex_thread_element*)malloc(sizeof(struct mutex_thread_element));
	memset(elem, 0, sizeof(struct mutex_thread_element));
	elem->tid = tid;
	elem->mutex_ptr = mutex_ptr;
	strcpy(elem->mutex_name, name);
	xdag_debug("check deadlock append (%12llx:%s) to try list\n", tid, name);
	LL_APPEND(try_list_mutex_thread, elem);
	pthread_mutex_unlock(&g_detect_mutex);
}

void apply_lock_after(uint64_t tid, pthread_mutex_t *mutex_ptr)
{
	pthread_mutex_lock(&g_detect_mutex);
	struct mutex_thread_element *elem = NULL;
	struct mutex_thread_element *tmp = NULL;
	LL_FOREACH_SAFE(try_list_mutex_thread, elem, tmp)
	{
		if(elem->tid == tid && elem->mutex_ptr == mutex_ptr) {
			LL_DELETE(try_list_mutex_thread, elem);
			xdag_debug("check deadlock move (%12llx:%s) from try to list\n", tid, elem->mutex_name);
			LL_APPEND(list_mutex_thread, elem);
			break;
		}
	}
	pthread_mutex_unlock(&g_detect_mutex);
}

void apply_unlock(uint64_t tid, pthread_mutex_t *mutex_ptr)
{
	pthread_mutex_lock(&g_detect_mutex);
	struct mutex_thread_element *elem = NULL;
	struct mutex_thread_element *tmp = NULL;
	LL_FOREACH_SAFE(list_mutex_thread, elem, tmp)
	{
		if(elem->tid == tid && elem->mutex_ptr == mutex_ptr) {
			xdag_debug("check deadlock remove (%12llx:%s) from list\n", tid, elem->mutex_name);
			LL_DELETE(list_mutex_thread, elem);
			free(elem);
			break;
		}
	}
	pthread_mutex_unlock(&g_detect_mutex);
}

//static void check_ring(struct mutex_thread_vertex * graph, struct mutex_thread_vertex *vertex, struct mutex_thread_element *path);
static void check_ring(struct mutex_thread_vertex * graph, struct mutex_thread_vertex *vertex, struct mutex_thread_element *path)
{
	if(!vertex || vertex->in_degrees == 0) {
		return;
	}
	
	struct mutex_thread_element *list_elem = NULL;
	struct mutex_thread_vertex *graph_elem = NULL;
	LL_FOREACH(vertex->depends, list_elem)
	{
		int found_ring = 0;
		struct mutex_thread_element *path_elem = NULL;
		LL_FOREACH(path, path_elem)
		{
			if(list_elem->tid == path_elem->tid) {
				found_ring = 1;
				break;
			}
		}
		
		if(found_ring) {
			xdag_err("Dead lock found!");
			LL_FOREACH(path, path_elem)
			{
				xdag_err("thread %12llx mutex %s ->", path_elem->tid, path_elem->mutex_name);
			}
			xdag_err("thread %12llx mutex %s", list_elem->tid, list_elem->mutex_name);
			continue;
		}
		
		path_elem = (struct mutex_thread_element*)malloc(sizeof(struct mutex_thread_element));
		memcpy(path_elem, list_elem, sizeof(struct mutex_thread_element));
		LL_APPEND(path, path_elem);
		
		LL_FOREACH(graph, graph_elem)
		{
			if(graph_elem->tid == list_elem->tid) {
				check_ring(graph, graph_elem, path);
			}
		}
		
		int delete = 0;
		struct mutex_thread_element *tmp = NULL;
		LL_FOREACH_SAFE(path, path_elem, tmp)
		{
			if(delete) {
				LL_DELETE(path, path_elem);
				free(path_elem);
			}
			if(path_elem == list_elem) {
				delete = 1;
			}
		}
	}
}

/*
 Record mutex lock/unlock action and threads in list, and generate oriented graph.
 If there are rings in oriented graph, it means there are dead locks somethere, 
 and prints the dependeneces for mutex between related threads.
 */
static void check_deadlock(void)
{
	pthread_mutex_lock(&g_detect_mutex);
	struct mutex_thread_vertex * graph = NULL;
	
	do {
		struct mutex_thread_element *elem1 = NULL;
		struct mutex_thread_element *elem2 = NULL;
		LL_FOREACH(try_list_mutex_thread, elem1)
		{
			LL_FOREACH(list_mutex_thread, elem2)
			{
				if(elem1->mutex_ptr == elem2->mutex_ptr) {
					struct mutex_thread_vertex *vertex = NULL;
					LL_FOREACH(graph, vertex)
					{
						if(elem1->tid == vertex->tid) {
							break;
						}
					}
					
					if (!vertex) {
						vertex = (struct mutex_thread_vertex*)malloc(sizeof(struct mutex_thread_vertex));
						memset(vertex, 0, sizeof(struct mutex_thread_vertex));
						vertex->tid = elem1->tid;
						LL_APPEND(graph, vertex);
					}
					
					xdag_debug("add dependence (%12llx:%s)->(%12llx:%s)\n", elem1->tid, elem1->mutex_name, elem2->tid, elem2->mutex_name);

					++vertex->in_degrees;
					
					struct mutex_thread_element * tmpelem = (struct mutex_thread_element*)malloc(sizeof(struct mutex_thread_element));
					memcpy(tmpelem, elem2, sizeof(struct mutex_thread_element));
					LL_APPEND(vertex->depends, tmpelem);
				}
			}
		}
	} while (0);
	
	do {
		struct mutex_thread_vertex *elem = NULL;
		struct mutex_thread_element *path = NULL;
		LL_FOREACH(graph, elem)
		{
			check_ring(graph, elem, path);
		}
		
		struct mutex_thread_element *tmp1 = NULL;
		struct mutex_thread_element *tmp2 = NULL;
		LL_FOREACH_SAFE(path, tmp1, tmp2)
		{
			LL_DELETE(path, tmp1);
			free(tmp1);
		}
	} while(0);
	
	do {
		struct mutex_thread_vertex *vertex, *tmpvertex;
		struct mutex_thread_element *elem, *tmpelem;
		LL_FOREACH_SAFE(graph, vertex, tmpvertex)
		{
			LL_FOREACH_SAFE(vertex->depends, elem, tmpelem)
			{
				LL_DELETE(vertex->depends, elem);
				free(elem);
			}
			free(vertex);
		}
	} while(0);
	
	pthread_mutex_unlock(&g_detect_mutex);
}

static void* check_deadlock_thread(void* argv)
{
	while(1) {
		xdag_debug("check dead lock thread loop...\n");
		check_deadlock();
		sleep(10);
	}
	
	return 0;
}

void start_check_deadlock_thread(void)
{
	pthread_t th;
	
	int err = pthread_create(&th, 0, check_deadlock_thread, NULL);
	if(err != 0) {
		xdag_err("create check_deadlock_thread failed! error : %s", strerror(err));
		return;
	}
	
	err = pthread_detach(th);
	if(err != 0) {
		xdag_err("detach check_deadlock_thread failed! error : %s", strerror(err));
		return;
	}
}

void test_deadlock(void)
{
	apply_lock_before(10000, (pthread_mutex_t*)1, "1");
	apply_lock_after(10000, (pthread_mutex_t*)1);
	apply_unlock(10000, (pthread_mutex_t*)1);
	
	apply_lock_before(10002, (pthread_mutex_t*)2, "2");
	apply_lock_after(10002, (pthread_mutex_t*)2);
//	apply_unlock(10002, 2);
	
	apply_lock_before(10003, (pthread_mutex_t*)3, "3");
	apply_lock_after(10003, (pthread_mutex_t*)3);
//	apply_unlock(10003, 3);
	
	apply_lock_before(20001, (pthread_mutex_t*)2, "2");
	apply_lock_before(30001, (pthread_mutex_t*)3, "3");
	
	apply_lock_before(40001, (pthread_mutex_t*)1, "1");
	apply_lock_after(40001, (pthread_mutex_t*)1);
	apply_lock_before(40001, (pthread_mutex_t*)2, "2");
	
	apply_lock_before(40002, (pthread_mutex_t*)2, "2");
	apply_lock_after(40002, (pthread_mutex_t*)2);
	apply_lock_before(40002, (pthread_mutex_t*)1, "1");
	
	
	check_deadlock();
}

uint64_t get_timestamp(void)
{
    struct timeval tp;
    
    gettimeofday(&tp, 0);
    
    return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

static char g_xdag_current_path[4096] = {0};

void xdag_init_path(char *path)
{
#ifdef _WIN32
	char szPath[MAX_PATH];
	char szBuffer[MAX_PATH];
	char *pszFile;

	GetModuleFileName(NULL, (LPTCH)szPath, sizeof(szPath) / sizeof(*szPath));
	GetFullPathName((LPTSTR)szPath, sizeof(szBuffer) / sizeof(*szBuffer), (LPTSTR)szBuffer, (LPTSTR*)&pszFile);
	*pszFile = 0;

	strcpy(g_xdag_current_path, szBuffer);
#else
	char pathcopy[PATH_MAX] = {0};
	strcpy(pathcopy, path);
	char *prefix = dirname(pathcopy);
	if (*prefix != '/' && *prefix != '\\') {
		char buf[PATH_MAX] = {0};
		getcwd(buf, PATH_MAX);
		sprintf(g_xdag_current_path, "%s/%s", buf, prefix);
	} else {
		sprintf(g_xdag_current_path, "%s", prefix);
	}
#if defined (__APPLE__)|| defined (__MACOS__)
	free(prefix);
#endif
#endif

	const size_t pathLen = strlen(g_xdag_current_path);
	if (pathLen == 0 || g_xdag_current_path[pathLen - 1] != *DELIMITER) {
		g_xdag_current_path[pathLen] = *DELIMITER;
		g_xdag_current_path[pathLen + 1] = 0;
	}
}

FILE* xdag_open_file(const char *path, const char *mode)
{
	char abspath[1024] = {0};
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
	char abspath[1024] = {0};
	sprintf(abspath, "%s%s", g_xdag_current_path, path);
	struct stat st;
	return !stat(abspath, &st);
}

int xdag_mkdir(const char *path)
{
	char abspath[1024] = {0};
	sprintf(abspath, "%s%s", g_xdag_current_path, path);

#if defined(_WIN32)
	return _mkdir(abspath);
#else 
	return mkdir(abspath, 0770);
#endif	
}

long double log_difficulty2hashrate(long double log_diff)
{
        return ldexpl(expl(log_diff), -58)*(0.65);
}

void xdag_str_toupper(char *str)
{
	while(*str) {
		*str = toupper((unsigned char) *str);
		str++;
	}
}

void xdag_str_tolower(char *str)
{
	while(*str) {
		*str = tolower((unsigned char) *str);
		str++;
	}
}

char *xdag_basename(char *path)
{
	#if defined(_WIN32)
		char *ptr;
		while ((ptr = strchr(path, '/')) || (ptr = strchr(path, '\\'))) {
			path = ptr + 1;
		}
		return strdup(ptr);
	#else
		return strdup(basename(path));
	#endif
}

char *xdag_filename(char *_filename)
{
	char * filename = xdag_basename(_filename);
	char * ext = strchr(filename, '.');

	if(ext) {
		*ext = 0;
	}

	return filename;
}

