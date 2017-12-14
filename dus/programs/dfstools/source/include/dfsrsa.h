#ifndef DFSTOOLS_DFSRSA_H_INCLUDED
#define DFSTOOLS_DFSRSA_H_INCLUDED

/* Определения функций для длинной арифметики и криптографии RSA, T8.505-T11.316; $DVS:time$ */

#ifdef __DuS__
#define DFSRSA_EXT extern "dfstools/dfsrsa.o"
#else
#define DFSRSA_EXT extern
#endif

#include <stdint.h>

#ifdef __DuS__
typedef uint16_t	dfsrsa_t;
typedef uint32_t	dfsrsa_long_t;
typedef int32_t		dfsrsa_slong_t;
#else
typedef uint32_t	dfsrsa_t;
typedef uint64_t	dfsrsa_long_t;
typedef int64_t		dfsrsa_slong_t;
#endif

/* сгенерировать открытый и закрытый ключи длины keylen чисел dfsrsa_t, keylen должно
   быть кратно 4, массив pubkey должен быть предварительно заполнен случайными
   числами, алгоритм не использует никакой другой случайной информации;
   при ошибке возвращает -1
*/
DFSRSA_EXT int dfsrsa_keygen(dfsrsa_t *privkey, dfsrsa_t *pubkey, int keylen);

/* закодировать/раскодировать сообщение, используя соответствующий открытый/закрытый
   ключ, datalen и keylen измеряются в числах dfsrsa_t, ключ key должен быть ранее
   сгенерирован функцией dfsrsa_keygen, datalen должно быть кратно половине от keylen;
   в каждой порции сообщения (длиной keylen/2 чисел dfsrsa_t) старший бит должен быть 0;
   итоговое сообщение помещается в тот же массив data, что и исходное;
   при ошибке возвращает -1
*/
DFSRSA_EXT int dfsrsa_crypt(dfsrsa_t *data, int datalen, dfsrsa_t *key, int keylen);

/* сравнивает два длинных числа и возвращает -1, 0, 1
*/
DFSRSA_EXT int dfsrsa_cmp(dfsrsa_t *left, dfsrsa_t *right, int len);

/* складывает два длинных числа, перенос возвращает (0 или 1)
*/
DFSRSA_EXT int dfsrsa_add(dfsrsa_t *sum, dfsrsa_t *add1, dfsrsa_t *add2, int len);

DFSRSA_EXT int dfsrsa_divmod(dfsrsa_t *mod, int mlen, dfsrsa_t *div, int len, dfsrsa_t *quotient);

#undef DFSRSA_EXT

#endif
