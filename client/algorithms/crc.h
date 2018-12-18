#ifndef _CRC_H
#define _CRC_H

// header file of CRC-library 

#include <stdio.h>

/* initialization of the internal CRC-table (with memory allocation) */
extern int crc_init(void);

/* creation of the table in the external array of length of 256 double words */
extern int crc_makeTable(unsigned table[256]);

/* добавить к накопленному CRC новые данные, содержащиеся в массиве buf
   длины len; возвращает новый CRC; начальное значение CRC=0 */
extern unsigned crc_addArray(unsigned char *buf, unsigned len, unsigned crc);

/* добавить к накопленному CRC новые данные, содержащиеся в файле f, но не
	более len байт; возвращает новый CRC; начальное значение CRC=0 */
extern unsigned crc_addFile(FILE *f, unsigned len, unsigned crc);

/* calculates CRC of the array */
#define crc_of_array(buf,len)	crc_addArray(buf,len,0)

/* calculates CRC of the file */
#define crc_of_file(f)		crc_addFile(f,-1,0)

#endif
