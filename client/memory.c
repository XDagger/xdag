/* работа с памятью, T13.816-T13.889 $DVS:time$ */

#include <stdlib.h>
#include <stdint.h>
#include "memory.h"
#include "utils/log.h"

#if defined(_WIN32) || defined(_WIN64)

void xdag_mem_tempfile_path(const char *tempfile_path)
{
}

int xdag_mem_init(size_t size)
{
	return 0;
}

void *xdag_malloc(size_t size)
{
	return malloc(size);
}

void xdag_free(void *mem)
{
	free(mem);
}

void xdag_mem_finish(void)
{
}

int xdag_free_all(void)
{
	return -1;
}

#else

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>

#define MEM_PORTION     ((size_t)1 << 25)

static int g_fd = -1;
static size_t g_pos = 0, g_fsize = 0, g_size = 0;
static void *g_mem;
static pthread_mutex_t g_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_tmpfile_path[1024] = "";
static char g_tmpname[64] = "xdag-tmp-XXXXXX";

void xdag_mem_tempfile_path(const char *tempfile_path)
{
	strcpy(g_tmpfile_path, tempfile_path);
}

int xdag_mem_init(size_t size)
{
	char tmpfilename[1024];

	if (!size) {
		return 0;
	}
	if(strcmp(g_tmpfile_path,"RAM") == 0) {
		/* This will leave g_fd as -1, and malloc will then be called in xdag_malloc instead of pointer into mmap */
		return 0;
	}

	size--;
	size |= MEM_PORTION - 1;
	size++;

	printf("%s , %s\n",g_tmpfile_path, g_tmpname);
	sprintf(tmpfilename, "%s%s", g_tmpfile_path, g_tmpname);
	g_fd = mkstemp(tmpfilename);
	if (g_fd < 0) {
		xdag_fatal("Unable to create temporary file %s errno:%d", tmpfilename, errno);
		return -1;
	}

	g_mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
	if (g_mem == MAP_FAILED) {
		close(g_fd); return -1;
	}

	g_size = size;
	
	return 0;
}

void *xdag_malloc(size_t size)
{
	uint8_t *res;

	if (g_fd < 0) {
		return malloc(size);
	}

	if (!size) {
		return 0;
	}

	pthread_mutex_lock(&g_mem_mutex);
	
	size--;
	size |= 0xf;
	size++;

	while (g_pos + size > g_fsize) {
		if (g_fsize + MEM_PORTION > g_size) {
			pthread_mutex_unlock(&g_mem_mutex);
			return 0;
		}
		g_fsize += MEM_PORTION;
		ftruncate(g_fd, g_fsize);
	}

	res = (uint8_t*)g_mem + g_pos;
	g_pos += size;
	
	pthread_mutex_unlock(&g_mem_mutex);
	
	return res;
}

void xdag_free(void *mem)
{
}

void xdag_mem_finish(void)
{
	if (g_fd < 0) return;
	
	pthread_mutex_lock(&g_mem_mutex);
	
	munmap(g_mem, g_size);
	ftruncate(g_fd, 0);
	close(g_fd);
	remove(g_tmpname);
}

int xdag_free_all(void)
{
	g_pos = 0;
	return 0;
}

#endif

char** xdagCreateStringArray(int count, int stringLen)
{
	char** stringArray = malloc(count * sizeof(char*));
	for (int i = 0; i < count; ++i) {
		stringArray[i] = malloc(stringLen);
	}
	return  stringArray;
}

void xdagFreeStringArray(char** stringArray, int count)
{
	for (int i = 0; i < count; ++i) {
		free(stringArray[i]);
	}
	free(stringArray);
}
