#ifndef _DAR_CRC_H_INCLUDED
#define _DAR_CRC_H_INCLUDED

/* h-файл библиотеки CRC, T4.046-T9.267; $DVS:time$ */

#ifdef __DuS__
#define EXTERN extern "dar/crc_c.o"
#else
#include <stdio.h>
#define EXTERN extern
#endif

/* инициализация внутренней таблицы CRC (с выделением памяти) */
EXTERN int
	crc_init	(void),

/* построение таблицы во внешнем массиве длины 256 двойных слов */
	crc_makeTable	(unsigned table[256]);


EXTERN unsigned

/* добавить к накопленному CRC новые данные, содержащиеся в массиве buf
   длины len; возвращает новый CRC; начальное значение CRC=0 */
	crc_addArray	(unsigned char *buf, unsigned len, unsigned crc),

/* добавить к накопленному CRC новые данные, содержащиеся в файле f, но не
   более len байт; возвращает новый CRC; начальное значение CRC=0 */
	crc_addFile	(FILE *f, unsigned len, unsigned crc);

/* подсчитать CRC массива */
#define crc_of_array(buf,len)	crc_addArray(buf,len,0)

/* подсчитать CRC файла */
#define crc_of_file(f)		crc_addFile(f,-1,0)

#undef EXTERN

#endif
