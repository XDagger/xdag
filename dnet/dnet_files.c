/* dnet: remote copy files; T13.033-T13.448; $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CHEATCOIN
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif
#include "dnet_files.h"
#include "dnet_command.h"
#include "dnet_threads.h"
#include "dnet_database.h"

#define FILE_HEAD_SEQ		0xFFFFFFFFFFFFF000ull

/* копирование из источника from в назначение to; источник и назначение заданы в формате [host:]port */
int dnet_file_command(const char *from, const char *to, const char *param, struct dnet_output *out) {
#ifndef CHEATCOIN
	char hostname[DNET_HOST_NAME_MAX];
	struct dnet_host *h;
	const char *s;
	int res;
	if ((s = strchr(from, ':'))) {
		char cmd[DNET_COMMAND_MAX];
		memcpy(hostname, from, s - from);
		hostname[s - from] = 0;
		h = dnet_get_host_by_name(hostname);
		if (!h) return 1;
		if (strchr(to, ':'))
			sprintf(cmd, "%.*s copy \"%s\" \"%s\"", (int)(s - from), from, s + 1, to);
		else {
			h = dnet_get_self_host();
			if (h->name_len)
				sprintf(cmd, "%.*s copy \"%s\" \"%.*s:%s\"", (int)(s - from), from, s + 1, h->name_len, h->name, to);
			else
				sprintf(cmd, "%.*s copy \"%s\" \"%08X:%s\"", (int)(s - from), from, s + 1, h->crc32, to);
		}
		if (param) sprintf(cmd + strlen(cmd), " \"%s\"", param);
		res = dnet_command(cmd, out);
		if (res) res = res << 4 | 2;
		return res;
	} else {
		struct dnet_thread *t;
		struct stat st;
		if (stat(from, &st)) return 3;
		s = strchr(to,':');
		if (!s) return 4;
		memcpy(hostname, to, s - to);
		hostname[s - to] = 0;
		h = dnet_get_host_by_name(hostname);
		if (!h) return 5;
		to = s + 1;
		t = (struct dnet_thread *)malloc(sizeof(struct dnet_thread));
		if (!t) return 6;
		t->type = DNET_THREAD_STREAM;
		t->st.pkt_type = DNET_PKT_FILE_OP;
		t->st.crc_to = h->crc32;
		t->st.to_exit = 0;
		dnet_generate_stream_id(&t->st.id);
		t->st.file_from = strdup(from);
		t->st.file_to = strdup(to);
		t->st.file_param = (param ? strdup(param) : 0);
		res = dnet_thread_create(t);
		if (res) {
			t->to_remove = 1;
			res = res << 4 | 7;
		}
		return res;
	}
#else
	return -1;
#endif
}

#ifndef CHEATCOIN
struct copy_data {
	char path[PATH_MAX];
	struct stat stat;
	struct dnet_thread *t;
	struct dnet_packet_stream *st;
	volatile uint8_t *to_exit;
	int from_len;
	int to_len;
	int path_len;
};

static int put_attributes(uint8_t *buf, unsigned mode, unsigned long sec, unsigned nsec) {
	sprintf((char *)buf, "%o %lu.%09u ", mode, sec, nsec);
	return strlen((char *)buf);
}

static int copy_elem(struct copy_data *d);

#ifdef st_mtime
#define st_mtime_nsec st_mtim.tv_nsec
#endif

static int copy_file_dir(struct copy_data *d, int isdir) {
	uint8_t *ptr;
	unsigned long sec = d->stat.st_mtime;
	unsigned mode = d->stat.st_mode, nsec = d->stat.st_mtime_nsec;
	int err, i;
	for (i = 0; i < 2; ++i) {
		if (*d->to_exit) return 1;
		ptr = d->st->data;
		memcpy(ptr, (i ? "MOD " : (isdir ? "DIR " : (d->t->st.file_param && *d->t->st.file_param != '0' ? "ADD " : "PUT "))), 4);
		ptr += 4;
		ptr += put_attributes(ptr, mode, sec, nsec);
		memcpy(ptr, d->t->st.file_to, d->to_len);
		ptr += d->to_len;
		strcpy((char *)ptr, d->path + d->from_len);
		ptr += d->path_len - d->from_len + 1;
		d->st->header.length = DNET_PKT_STREAM_MIN_LEN + (ptr - d->st->data);
		d->st->ack++;
		d->t->st.ack += !i;
		d->st->seq = FILE_HEAD_SEQ;
		err = dnet_send_stream_packet(d->st, 0);
		if (err) return err << 4 | 2;
		if (i) break;
		if (isdir) {
			DIR *dir = opendir(d->path);
			struct dirent *de;
			if (!dir) return 3;
			while ((de = readdir(dir))) {
				int len = strlen(de->d_name);
				if (*d->to_exit) return 4;
				if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
				d->path[d->path_len] = '/';
				strcpy(d->path + d->path_len + 1, de->d_name);
				d->path_len += len + 1;
				err = copy_elem(d);
				if (err) { closedir(dir); return err << 4 | 5; }
				d->path_len -= len + 1;
				d->path[d->path_len] = 0;
			}
			closedir(dir);
		} else {
			FILE *f = fopen(d->path, "rb");
			size_t bytes_per_sec = ULONG_MAX, sent = 0;
			ssize_t res;
			time_t t0 = time(0), t, min_low_speed = 0;
			if (!f) return 3;
			d->st->seq = 0;
			if (d->t->st.file_param) sscanf(d->t->st.file_param, "%lu,%lu,%ld", &d->st->seq, &bytes_per_sec, &min_low_speed);
			if (d->st->seq) fseek(f, d->st->seq, SEEK_SET);
			do {
				if (*d->to_exit) { fclose(f); return 4; }
				res = fread(d->st->data, 1, DNET_PKT_STREAM_DATA_MAX, f);
				if (res < 0) { fclose(f); return 5; }
				if (res == 0) break;
				d->st->header.length = DNET_PKT_STREAM_MIN_LEN + res;
				err = dnet_send_stream_packet(d->st, 0);
				if (err) { fclose(f); return err << 4 | 6; }
				d->st->seq += res;
				d->t->st.seq += res;
				sent += res;
				t = time(0) - t0;
				if (t >= min_low_speed && sent / bytes_per_sec >= t + 1)
					sleep(sent / bytes_per_sec - t);
			} while (res == DNET_PKT_STREAM_DATA_MAX);
			fclose(f);
		}
	}
	return 0;
}

static int copy_link(struct copy_data *d) {
	uint8_t *ptr;
	ssize_t res;
	int err;
	if (*d->to_exit) return 1;
	ptr = d->st->data;
	memcpy(ptr, "LNK ", 4);
	ptr += 4;
	ptr += put_attributes(ptr, d->stat.st_mode, d->stat.st_mtime, d->stat.st_mtime_nsec);
	memcpy(ptr, d->t->st.file_to, d->to_len);
	ptr += d->to_len;
	strcpy((char *)ptr, d->path + d->from_len);
	ptr += d->path_len - d->from_len + 1;
	res = readlink(d->path, (char *)ptr, DNET_PKT_STREAM_DATA_MAX - (ptr - d->st->data));
	if (res < 0) return 2;
	ptr += res;
	*ptr = 0;
	ptr++;
	d->st->header.length = DNET_PKT_STREAM_MIN_LEN + (ptr - d->st->data);
	d->st->ack++;
	d->t->st.ack++;
	d->st->seq = FILE_HEAD_SEQ;
	err = dnet_send_stream_packet(d->st, 0);
	if (err) err = err << 4 | 3;
	return err;
}

static int copy_elem(struct copy_data *d) {
	int err, n;
	if (*d->to_exit) return 1;
	if (lstat(d->path, &d->stat)) return 2;
	if (S_ISDIR(d->stat.st_mode)) err = copy_file_dir(d, 1), n = 3;
	else if (S_ISREG(d->stat.st_mode)) err = copy_file_dir(d, 0), n = 4;
	else if (S_ISLNK(d->stat.st_mode)) err = copy_link(d), n = 5;
	else err = 0;
	if (err) err = err << 4 | n;
	return err;
}

/* асинхронная отправка файла или каталога */
int dnet_file_thread(struct dnet_thread *t, struct dnet_packet_stream *st) {
	struct copy_data d;
	d.t = t;
	d.st = st;
	d.to_exit = &t->st.to_exit;
	strcpy(d.path, t->st.file_from);
	d.from_len = d.path_len = strlen(d.path);
	d.to_len = strlen(t->st.file_to);
	return copy_elem(&d);
}

struct file_receiver {
	struct dnet_stream_id id;
	FILE *f;
	uint64_t seq, ack;
	time_t last;
	int active;
};

#define N_FILE_RECEIVERS	16
#define FILE_TIMEOUT		10

static pthread_mutex_t g_file_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct file_receiver *g_file_receivers = 0;

static int process_head_packet(struct dnet_packet_stream *st, struct file_receiver *fr) {
	char *cmd, *str, *lasts;
	unsigned long m;
	unsigned n;
	mode_t mode;
//	printf("[%s]\n", st->data);fflush(stdout);
	cmd = strtok_r((char *)st->data, " ", &lasts);
	if (!cmd) return 3;
	str = strtok_r(0, " ", &lasts);
	if (!str) return 4;
	if (sscanf(str, "%o", &n) != 1) return 5;
	mode = n;
	str = strtok_r(0, " ", &lasts);
	if (!str) return 6;
	if (sscanf(str, "%lu.%u", &m, &n) != 2) return 7;
	str = str + strlen(str) + 1;
	if (!strcmp(cmd, "MOD")) {
		struct timeval t[2];
		chmod(str, mode);
		t[0].tv_sec = t[1].tv_sec = m;
		t[0].tv_usec = t[1].tv_usec = n / 1000;
		utimes(str, t);
		fr->active = 0;
	} else if (!strcmp(cmd, "PUT")) {
		int fd = open(str, O_WRONLY | O_CREAT | O_TRUNC, mode | 0700);
		if (fd < 0) return 8;
		fr->f = fdopen(fd, "w");
		if (!fr->f) return 9;
		fr->seq = 0, fr->ack = st->ack;
	} else if (!strcmp(cmd, "ADD")) {
		int fd = open(str, O_WRONLY | O_CREAT | O_APPEND, mode | 0700);
		if (fd < 0) return 8;
		fr->f = fdopen(fd, "a");
		if (!fr->f) return 9;
		fseek(fr->f, 0, SEEK_END);
		fr->seq = ftell(fr->f), fr->ack = st->ack;
	} else if (!strcmp(cmd, "DIR")) {
		mkdir(str, mode | 0700);
		fr->active = 0;
	} else if (!strcmp(cmd, "LNK")) {
		symlink(str + strlen(str) + 1, str);
#ifndef QDNET
		{
			struct timeval t[2];
			t[0].tv_sec = t[1].tv_sec = m;
			t[0].tv_usec = t[1].tv_usec = n / 1000;
			lutimes(str, t);
		}
#endif
		fr->active = 0;
	}
	return 0;
}
#endif

/* обработка принятого файлового пакета */
int dnet_process_file_packet(struct dnet_packet_stream *st) {
#ifndef CHEATCOIN
	struct file_receiver *fr;
	time_t t;
	int i, len;
	pthread_mutex_lock(&g_file_mutex);
	t = time(0);
	if (!g_file_receivers) {
		g_file_receivers = calloc(N_FILE_RECEIVERS, sizeof(struct file_receiver));
		if (!g_file_receivers) {
			fr = 0;
			goto found;
		}
	}
	for (i = 0; i < N_FILE_RECEIVERS; ++i) {
		fr = &g_file_receivers[i];
		if (fr->active && !memcmp(&fr->id, &st->id, sizeof(struct dnet_stream_id)))
			goto found;
	}
	for (i = 0; i < N_FILE_RECEIVERS; ++i) {
		fr = &g_file_receivers[i];
		if (!fr->active)
			goto found;
	}
	for (i = 0; i < N_FILE_RECEIVERS; ++i) {
		fr = &g_file_receivers[i];
		if (t - fr->last >= FILE_TIMEOUT)
			goto found;
	}
	fr = 0;
found:
	if (fr) {
		memcpy(&fr->id, &st->id, sizeof(struct dnet_stream_id));
		fr->active = 1, fr->last = t;
	}
	pthread_mutex_unlock(&g_file_mutex);
	if (!fr) return 1;
	if (st->seq == FILE_HEAD_SEQ) {
		int res;
		if (fr->f) { fclose(fr->f); fr->f = 0; }
		res = process_head_packet(st, fr);
		if (res) fr->active = 0;
		return res;
	}
	if (!fr->f || fr->ack != st->ack || fr->seq != st->seq) {
		if (fr->f) { fclose(fr->f); fr->f = 0; }
		fr->active = 0;
		return 2;
	}
	len = st->header.length - DNET_PKT_STREAM_MIN_LEN;
	if (len) {
		fwrite(st->data, 1, len, fr->f);
		fr->seq += len;
	}
#endif
	return 0;
}
