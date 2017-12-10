/* локальное хранилище, T13.663-T13.726 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include "storage.h"
#include "log.h"
#include "main.h"

#define STORAGE_DIR0			"storage%s"
#define STORAGE_DIR0_ARGS(t)	(g_cheatcoin_testnet ? "-testnet" : "")
#define STORAGE_DIR1			STORAGE_DIR0 "/%02x"
#define STORAGE_DIR1_ARGS(t)	STORAGE_DIR0_ARGS(t), (int)((t) >> 40)
#define STORAGE_DIR2			STORAGE_DIR1 "/%02x"
#define STORAGE_DIR2_ARGS(t)	STORAGE_DIR1_ARGS(t), (int)((t) >> 32) & 0xff
#define STORAGE_DIR3			STORAGE_DIR2 "/%02x"
#define STORAGE_DIR3_ARGS(t)	STORAGE_DIR2_ARGS(t), (int)((t) >> 24) & 0xff
#define STORAGE_FILE			STORAGE_DIR3 "/%02x.dat"
#define STORAGE_FILE_ARGS(t)	STORAGE_DIR3_ARGS(t), (int)((t) >> 16) & 0xff
#define SUMS_FILE				"sums.dat"

static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;
static int in_adding_all = 0;

static int correct_storage_sum(const char *path, int pos, const struct cheatcoin_storage_sum *sum, int add) {
	struct cheatcoin_storage_sum sums[256];
	FILE *f = fopen(path, "r+");
	if (f) {
		if (fread(sums, sizeof(struct cheatcoin_storage_sum), 256, f) != 256)
			{ fclose(f); cheatcoin_err("Storag: sums file %s corrupted", path); return -1; }
		rewind(f);
	} else {
		f = fopen(path, "w");
		if (!f) { cheatcoin_err("Storag: can't create file %s", path); return -1; }
		memset(sums, 0, sizeof(sums));
	}
	if (!add) {
		if (sums[pos].size == sum->size && sums[pos].sum == sum->sum) { fclose(f); return 0; }
		if (sums[pos].size || sums[pos].sum) { fclose(f); cheatcoin_err("Storag: corrupted, sums file %s, pos %x", path, pos); return -1; }
	}
	sums[pos].size += sum->size;
	sums[pos].sum  += sum->sum;
	if (fwrite(sums, sizeof(struct cheatcoin_storage_sum), 256, f) != 256)
		{ fclose(f); cheatcoin_err("Storag: can't write file %s", path); return -1; }
	fclose(f);
	return 1;
}

static int correct_storage_sums(cheatcoin_time_t t, const struct cheatcoin_storage_sum *sum, int add) {
	char path[256];
	int res;
	sprintf(path, STORAGE_DIR3 "/" SUMS_FILE, STORAGE_DIR3_ARGS(t));
	res = correct_storage_sum(path, (t >> 16) & 0xff, sum, add);
	if (res <= 0) return res;
	sprintf(path, STORAGE_DIR2 "/" SUMS_FILE, STORAGE_DIR2_ARGS(t));
	res = correct_storage_sum(path, (t >> 24) & 0xff, sum, 1);
	if (res <= 0) return res;
	sprintf(path, STORAGE_DIR1 "/" SUMS_FILE, STORAGE_DIR1_ARGS(t));
	res = correct_storage_sum(path, (t >> 32) & 0xff, sum, 1);
	if (res <= 0) return res;
	sprintf(path, STORAGE_DIR0 "/" SUMS_FILE, STORAGE_DIR0_ARGS(t));
	res = correct_storage_sum(path, (t >> 40) & 0xff, sum, 1);
	if (res <= 0) return res;
	return 0;
}

/* сохранить блок в локальное хранилище, возвращает его номер или -1 при ошибке */
int64_t cheatcoin_storage_save(const struct cheatcoin_block *b) {
	struct cheatcoin_storage_sum s;
	char path[256];
	FILE *f;
	int64_t res;
	int j;
	if (in_adding_all) return -1;
	sprintf(path, "mkdir -p " STORAGE_DIR3, STORAGE_DIR3_ARGS(b->field[0].time));
	system(path);
	sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(b->field[0].time));
	pthread_mutex_lock(&storage_mutex);
	f = fopen(path, "a");
	if (f) {
		res = ftell(f);
		fwrite(b, sizeof(struct cheatcoin_block), 1, f);
		fclose(f);
		s.size = sizeof(struct cheatcoin_block);
		s.sum = 0;
		for (j = 0; j < sizeof(struct cheatcoin_block) / sizeof(uint64_t); ++j)
			s.sum += ((uint64_t *)b)[j];
		if (correct_storage_sums(b->field[0].time, &s, 1)) res = -1;
	} else res = -1;
	pthread_mutex_unlock(&storage_mutex);
	return res;
}

/* прочитать из локального хранилища блок с данным временем и номером; записать его в буфер или возвратить постоянную ссылку, 0 при ошибке */
struct cheatcoin_block *cheatcoin_storage_load(cheatcoin_time_t time, uint64_t pos, struct cheatcoin_block *buf) {
	char path[256];
	FILE *f;
	sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(time));
	pthread_mutex_lock(&storage_mutex);
	f = fopen(path, "r");
	if (f) {
		if (fseek(f, pos, SEEK_SET) < 0 || fread(buf, sizeof(struct cheatcoin_block), 1, f) != 1) buf = 0;
		fclose(f);
	} else buf = 0;
	pthread_mutex_unlock(&storage_mutex);
	return buf;
}

#define bufsize (0x100000 / sizeof(struct cheatcoin_block))

/* вызвать callback для всех блоков из хранилища, попадающих с данный временной интервал; возвращает число блоков */
uint64_t cheatcoin_load_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data, void *(*callback)(void *, void *)) {
	struct cheatcoin_block buf[bufsize];
	struct cheatcoin_storage_sum s;
	char path[256];
	struct stat st;
	FILE *f;
	uint64_t sum = 0, pos = 0, mask;
	int64_t i, j, todo;
	s.size = s.sum = 0;
	while (start_time < end_time) {
		sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(start_time));
		pthread_mutex_lock(&storage_mutex);
		f = fopen(path, "r");
		if (f) {
			if (fseek(f, pos, SEEK_SET) < 0) todo = 0;
			else todo = fread(buf, sizeof(struct cheatcoin_block), bufsize, f);
			fclose(f);
		} else todo = 0;
		pthread_mutex_unlock(&storage_mutex);
		for (i = 0; i < todo; ++i, pos += sizeof(struct cheatcoin_block)) {
			s.size += sizeof(struct cheatcoin_block);
			for (j = 0; j < sizeof(struct cheatcoin_block) / sizeof(uint64_t); ++j)
				s.sum += ((uint64_t *)(buf + i))[j];
			if (buf[i].field[0].time >= start_time && buf[i].field[0].time < end_time) {
				buf[i].field[0].transport_header = pos;
				sum++;
				if (callback(buf + i, data)) return sum;
			}
		}
		if (todo != bufsize) {
			if (f) {
				int res;
				pthread_mutex_lock(&storage_mutex);
				res = correct_storage_sums(start_time, &s, 0);
				pthread_mutex_unlock(&storage_mutex);
				if (res) break;
				s.size = s.sum = 0;
				mask = (1l << 16) - 1;
			}
			else if (sprintf(path, STORAGE_DIR3, STORAGE_DIR3_ARGS(start_time)), !stat(path, &st)) mask = (1l << 16) - 1;
			else if (sprintf(path, STORAGE_DIR2, STORAGE_DIR2_ARGS(start_time)), !stat(path, &st)) mask = (1l << 24) - 1;
			else if (sprintf(path, STORAGE_DIR1, STORAGE_DIR1_ARGS(start_time)), !stat(path, &st)) mask = (1l << 32) - 1;
			else mask = (1l << 40) - 1;
			start_time |= mask;
			start_time++;
			pos = 0;
		}
	}
	return sum;
}

/* в массив sums помещает суммы блоков по отрезку от start до end, делённому на 16 частей; end - start должно быть вида 16^k */
int cheatcoin_load_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]) {
	struct cheatcoin_storage_sum buf[256];
	char path[256];
	FILE *f;
	int i, level;
	end_time -= start_time;
	if (!end_time || end_time & (end_time - 1) || end_time & 0xFFFEEEEEEEEFFFFFl) return -1;
	for (level = -6; end_time; level++, end_time >>= 4){}
		 if (level < 2) sprintf(path, STORAGE_DIR3 "/" SUMS_FILE, STORAGE_DIR3_ARGS(start_time & 0xffffff000000l));
	else if (level < 4) sprintf(path, STORAGE_DIR2 "/" SUMS_FILE, STORAGE_DIR2_ARGS(start_time & 0xffff00000000l));
	else if (level < 6) sprintf(path, STORAGE_DIR1 "/" SUMS_FILE, STORAGE_DIR1_ARGS(start_time & 0xff0000000000l));
	else                sprintf(path, STORAGE_DIR0 "/" SUMS_FILE, STORAGE_DIR0_ARGS(start_time & 0x000000000000l));
	f = fopen(path, "r");
	if (f) { fread(buf, sizeof(struct cheatcoin_storage_sum), 256, f); fclose(f); }
	else memset(buf, 0, sizeof(buf));
	if (level & 1) {
		memset(sums, 0, 16 * sizeof(struct cheatcoin_storage_sum));
		for (i = 0; i < 256; ++i) sums[i >> 4].size += buf[i].size, sums[i >> 4].sum += buf[i].sum;
	} else memcpy(sums, buf + (start_time >> ((level + 4) * 4) & 0xf0), 16 * sizeof(struct cheatcoin_storage_sum));
	return 1;
}

/* завершает работу с хранилищем */
void cheatcoin_storage_finish(void) {
	pthread_mutex_lock(&storage_mutex);
}
