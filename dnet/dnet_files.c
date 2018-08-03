/* dnet: remote copy files; T13.033-T13.808; $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dnet_files.h"
#include "dnet_command.h"
#include "dnet_threads.h"
#include "dnet_database.h"

#define FILE_HEAD_SEQ		0xFFFFFFFFFFFFF000ull

/* копирование из источника from в назначение to; источник и назначение заданы в формате [host:]port */
int dnet_file_command(const char *from, const char *to, const char *param, struct dnet_output *out) {
	return -1;
}

/* обработка принятого файлового пакета */
int dnet_process_file_packet(struct dnet_packet_stream *st) {
	return 0;
}
