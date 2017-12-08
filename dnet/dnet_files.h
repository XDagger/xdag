/* dnet: remote copy files; T13.033-T13.426; $DVS:time$ */

#ifndef DNET_FILES_H_INCLUDED
#define DNET_FILES_H_INCLUDED

#include "dnet_log.h"
#include "dnet_threads.h"

/* копирование из источника from в назначение to; источник и назначение заданы в формате [host:]port */
extern int dnet_file_command(const char *from, const char *to, const char *param, struct dnet_output *out);

/* асинхронная отправка файла или каталога */
extern int dnet_file_thread(struct dnet_thread *t, struct dnet_packet_stream *st);

/* обработка принятого файлового пакета */
extern int dnet_process_file_packet(struct dnet_packet_stream *st);

#endif
