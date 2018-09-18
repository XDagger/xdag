/* memory managment, T13.816-T14.423 $DVS:time$ */

#include <stdlib.h>
#include <stdint.h>
#include "memory.h"
#include "utils/log.h"

int g_use_tmpfile = 0;

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
#include <limits.h>

#define MEM_PORTION     	((size_t)1 << 25)
#define TMPFILE_TEMPLATE 	"xdag-tmp-XXXXXX"
#define TMPFILE_TEMPLATE_LEN	15

struct temp_file {
	int fd;
	void *mem;
	char tmpfile[PATH_MAX];
	struct temp_file *prev;
};

static size_t g_pos = 0, g_fsize = 0, g_size = 0;
static pthread_mutex_t g_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_tmpfile_path[PATH_MAX];
static struct temp_file *g_last_tmpfile = NULL;
void delete_actual_tmpfile(void);

void xdag_mem_tempfile_path(const char *tempfile_path)
{
	if (!tempfile_path)
		return;

	strncpy(g_tmpfile_path, tempfile_path, PATH_MAX);
}

int xdag_mem_init(size_t size)
{
	struct temp_file *tfile_node = NULL;
	if (!size) {
		return 0;
	}
	if(strcmp(g_tmpfile_path,"RAM") == 0) {
		/* This will leave g_use_tmpfile as 0, and malloc will then be called in xdag_malloc instead of pointer into mmap */
		return 0;
	}

	tfile_node = calloc(sizeof(struct temp_file),1);
	if(tfile_node == NULL){
		xdag_warn("Error: creation of tmp file failed because calloc failed [function: xdag_mem_init]");
		return -1;
	}

	if(g_last_tmpfile == NULL) {
		size--;
		size |= MEM_PORTION - 1;
		size++;
	}

	int wrote = snprintf(tfile_node->tmpfile, PATH_MAX,"%s%s", g_tmpfile_path, TMPFILE_TEMPLATE);
	if (wrote < 0){
		xdag_fatal("Error: Fail to write tmpfile");
		free(tfile_node);
		return -1;
	} else if (wrote == PATH_MAX){
		xdag_fatal("Error: Temporary file path exceed the max length that is %d characters", PATH_MAX);
		free(tfile_node);
		return -1;
	}
	tfile_node->fd = mkstemp(tfile_node->tmpfile);
	if (tfile_node->fd < 0) {
		xdag_fatal("Unable to create temporary file %s errno:%d", tfile_node->tmpfile, errno);
		free(tfile_node);
		return -1;
	}
	xdag_info("Temporary file created: %s\n", tfile_node->tmpfile);

	tfile_node->mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, tfile_node->fd, 0);
	if (tfile_node->mem == MAP_FAILED) {
		close(tfile_node->fd);
		free(tfile_node);
		return -1;
	}

	if(g_last_tmpfile == NULL) {
		g_size = size;
	} else {
		g_pos = 0;
		g_fsize = 0;
	}

	tfile_node->prev = g_last_tmpfile;
	g_last_tmpfile = tfile_node;

	g_use_tmpfile = 1;

	return 0;
}

void *xdag_malloc(size_t size)
{
	uint8_t *res;
	
	if (size <= 0) {
		return 0;
	}

	if (!g_use_tmpfile) {
		return malloc(size);
	}

	if(g_last_tmpfile == NULL){
		xdag_warn("Unexpected state [function xdag_malloc]");
		return malloc(size);
	}

	
	pthread_mutex_lock(&g_mem_mutex);
	
	size--;
	size |= 0xf;
	size++;

	while (g_pos + size > g_fsize) {
		if (g_fsize + MEM_PORTION > g_size) {
			int res = xdag_mem_init(g_size);
			if(res){
				pthread_mutex_unlock(&g_mem_mutex);
				return 0;
			}
		}
		g_fsize += MEM_PORTION;
		ftruncate(g_last_tmpfile->fd, g_fsize);
	}

	res = (uint8_t*)(g_last_tmpfile->mem) + g_pos;
	g_pos += size;
	
	pthread_mutex_unlock(&g_mem_mutex);
	
	return res;
}

void xdag_free(void *mem)
{
	if(!g_use_tmpfile) {
		free(mem);
	}
}

void xdag_mem_finish(void)
{
	if (!g_use_tmpfile) {
		return;
	}
	
	pthread_mutex_lock(&g_mem_mutex);
	for (struct temp_file *next = NULL;
			g_last_tmpfile != NULL;
			next = g_last_tmpfile, g_last_tmpfile = g_last_tmpfile->prev, free(next)) {
		delete_actual_tmpfile();
	}
}

int xdag_free_all(void)
{
	if (!g_use_tmpfile) {
		return -1;
	}
	for(struct temp_file *next = NULL;
			g_last_tmpfile != NULL && g_last_tmpfile->prev != NULL;
			next = g_last_tmpfile, g_last_tmpfile = g_last_tmpfile->prev, free(next)){
		delete_actual_tmpfile();
	}
	g_pos = 0;

	return 0;
}

void delete_actual_tmpfile(){
	munmap(g_last_tmpfile->mem, g_size);
	ftruncate(g_last_tmpfile->fd, 0);
	close(g_last_tmpfile->fd);
	remove(g_last_tmpfile->tmpfile);
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
