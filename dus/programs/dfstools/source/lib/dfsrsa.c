/* Функции для длинной арифметики и криптографии RSA. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dfsrsa.h"

const char
	DFSRSA_TITLE[]		= "DFS RSA library",
	DFSRSA_VERSION[]	= "T8.505-T13.012", /* $DVS:time$ */
	DFSRSA_COPYRIGHT[]	= "(C) 2012-2017 Daniel Cheatoshin (cheatoshin@mailinator.com)";

#define USE_MILLER_RABIN
#define USE_MONTGOMERY
#define USE_KARATSUBA

#if defined(__x86_64__)
# define USE_FAST_MULT
# define USE_FAST_MULT_FIXED
# define KARATSUBA_TRESHOLD 	64
# define TEST_MAX_BITS		4096
#elif defined(__arm__)
# if defined(__ARM_ARCH_3__)	// for D-Link DNS-320
#  define USE_FAST_MULT
#  define USE_FAST_MULT_FIXED
#  define KARATSUBA_TRESHOLD 	64
#  define TEST_MAX_BITS		2048 
# else				// for Raspberry Pi
#  define KARATSUBA_TRESHOLD 	32
#  define TEST_MAX_BITS		1024
# endif
#else
# define KARATSUBA_TRESHOLD 	32
# define TEST_MAX_BITS		2048
#endif



/***********
 * отладка *
 ***********/


/* #define DFSRSA_DEBUG */

#define DFSRSA_DEBUG_TOP	0x01
#define DFSRSA_DEBUG_GEN	0x02
#define DFSRSA_DEBUG_MILLER	0x04
#define DFSRSA_DEBUG_MONT	0x08
#define DFSRSA_DEBUG_INV	0x10
#define DFSRSA_DEBUG_MOD	0x20
#define DFSRSA_DEBUG_MODLOW	0x40
#define DFSRSA_DEBUG_KARATSUBA	0x80

#define DFSRSA_DEBUG_MODE	(0x80)
#define DFSRSA_DEBUG_PRINT_MAX	100

#ifdef DFSRSA_DEBUG

#include <stdio.h>

static int dfsrsa_debug(int mode, char *name, dfsrsa_t *arr, int len){
	static int count = 0;
	if (!(mode & DFSRSA_DEBUG_MODE)) return -1;
	if (count >= DFSRSA_DEBUG_PRINT_MAX) return -1;
	count++;
	printf("%16s = 0x", name);
	while (len--) {
	    printf("%08X", arr[len]);
	}
	printf("\n");
	fflush(stdout);
	return 0;
}

#define dfsrsa_debug(mode, name, arr, len) \
	dfsrsa_debug(DFSRSA_DEBUG_##mode, name, arr, len)

#else

#define dfsrsa_debug(level, name, arr, len)

#endif



/***********************
 * простейшие операции *
 ***********************/

/* кол-во бит в числе dfsrsa_t
*/
#define DFSRSA_T_BITS		(sizeof(dfsrsa_t) << 3)

/* возвращает знак длинного числа: 1, если < 0; 0, если >= 0
*/
#define dfsrsa_sign(num, len)	((num)[(len) - 1] >> (DFSRSA_T_BITS - 1))

/* копирует длинное число
*/
static void dfsrsa_copy(dfsrsa_t *to, dfsrsa_t *from, int len){
	memcpy(to, from, len * sizeof(dfsrsa_t));
}

/* устанавливает длинное число равным value
*/
static void dfsrsa_set(dfsrsa_t *res, int len, dfsrsa_t value){
	if (len > 1) {
	    memset(res + 1, 0, (len - 1) * sizeof(dfsrsa_t));
	}
	*res = value;
}

/* возвращает кол-во значащих цифр (цифра - число dfsrsa_t), отбрасывая старшие нули
*/
static int dfsrsa_getlen(dfsrsa_t *num, int len) {
	num += len;
	while (len-- && !*--num);
	return len + 1;
}

/* сравнивает два длинных числа и возвращает -1, 0, 1
*/
int dfsrsa_cmp(dfsrsa_t *left, dfsrsa_t *right, int len) {
	left += len, right += len;
	while (len-- && *--left == *--right);
	if (len < 0) return 0;
	else if (*left < *right) return -1;
	else return 1;
}

/* к длинному числу прибавляет данное короткое, перенос отбрасывает
*/
static void dfsrsa_addeq(dfsrsa_t *sum, dfsrsa_t small, int len){
	dfsrsa_long_t res = small;
	while (len-- && res) {
	    res += *sum, *sum++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	}
}

/* складывает два длинных числа, перенос возвращает (0 или 1)
*/
int dfsrsa_add(dfsrsa_t *sum, dfsrsa_t *add1, dfsrsa_t *add2, int len){
	dfsrsa_long_t res = 0;
	while (len--) {
	    res += (dfsrsa_long_t)*add1++ + *add2++;
	    *sum++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	}
	return (int)res;
}

/* складывает два длинных числа и перенос, новый перенос возвращает
*/
static dfsrsa_t dfsrsa_adc(dfsrsa_t *sum, dfsrsa_t *add1, dfsrsa_t *add2,
		dfsrsa_t carry, int len){
	dfsrsa_long_t res = carry;
	while (len--) {
	    res += (dfsrsa_long_t)*add1++ + *add2++;
	    *sum++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	}
	return (dfsrsa_t)res;
}

/* вычитает из одного длинного числа другое, перенос возвращает (0 или -1)
*/
static int dfsrsa_sub(dfsrsa_t *sub, dfsrsa_t *from, dfsrsa_t *to, int len){
	dfsrsa_slong_t res = 0;
	while (len--) {
	    res += (dfsrsa_long_t)*from++ - *to++;
	    *sub++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	}
	return (int)res;
}

/* сдвигает длинное число на 1 разряд вправо, в старший бит записывает carry;
   младший бит возвращает
*/
static int dfsrsa_shr1(dfsrsa_t *num, int len, int carry){
	int next_carry;
	num += len;
	while (len--) {
	    next_carry = *--num & 1, *num >>= 1;
	    *num |= (dfsrsa_t)carry << (DFSRSA_T_BITS - 1);
	    carry = next_carry;
	}
	return carry;
}



/********************
 * методы умножения *
 ********************/


/* умножает число big длины len на число small и результат прибавляет к массиву sum
   длины len + 1, перенос записывает в len-й разряд sum 
*/
static void dfsrsa_muladd(dfsrsa_t *sum, dfsrsa_t *big, dfsrsa_t small, int len){
	dfsrsa_long_t res = 0;
	while (len--) {
	    res += (dfsrsa_long_t)*big++ * small + *sum;
	    *sum++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	}
	*sum = (dfsrsa_t)res;
}

/* умножает число big длины len на число small и результат вычитает из массива sub
   длины len, если есть перенос, то вычитает его из len-го элемента sub
*/
static void dfsrsa_mulsub(dfsrsa_t *sub, dfsrsa_t *big, dfsrsa_t small, int len){
	dfsrsa_long_t res = 0;
	while (len--) {
	    res += *sub - (dfsrsa_long_t)*big++ * small;
	    *sub++ = (dfsrsa_t)res, res >>= DFSRSA_T_BITS;
	    if (res) {
		res |= (dfsrsa_long_t)0-((dfsrsa_long_t)1 << DFSRSA_T_BITS);
	    }
	}
	if (res) *sub += (dfsrsa_t)res;
}

/* умножает два длинных числа, результат записывает в массив в два раза большей длины
*/
static void dfsrsa_mul(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2, int len) {
	int i = len;
	dfsrsa_set(prod, len, 0);
	while (i--) {
	    dfsrsa_muladd(prod++, mul1, *mul2++, len);
	}
}

#if defined(USE_FAST_MULT) && defined(__GNUC__) && defined(__x86_64__)

#define FASTMUL2_X86_64			\
		"lodsq\n"		\
		"mulq %%r8\n"		\
		"addq (%%rdi), %%rax\n"	\
		"adcq $0x0, %%rdx\n"	\
		"addq %%r9, %%rax\n"	\
		"adcq $0x0, %%rdx\n"	\
		"stosq\n"		\
		"movq %%rdx, %%r9\n"

#ifdef USE_FAST_MULT_FIXED

#define FASTMUL4_X86_64		FASTMUL2_X86_64  FASTMUL2_X86_64
#define FASTMUL8_X86_64		FASTMUL4_X86_64  FASTMUL4_X86_64
#define FASTMUL16_X86_64	FASTMUL8_X86_64  FASTMUL8_X86_64
#define FASTMUL32_X86_64	FASTMUL16_X86_64 FASTMUL16_X86_64
#define FASTMUL64_X86_64	FASTMUL32_X86_64 FASTMUL32_X86_64

static void dfsrsa_fastmul16_x86_64(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"xorq %%rax, %%rax\n"
		"movl $0x08, %%ecx\n"
		"rep  stosq\n"
		"movq %%rdx, %%rbx\n"
		"subq $0x40, %%rdi\n"
		"movl $0x08, %%ecx\n"
	     "1: movq (%%rbx), %%r8\n"
		"xor  %%r9, %%r9\n"
		FASTMUL16_X86_64
		"movq %%r9, (%%rdi)\n"
		"subq $0x40, %%rsi\n"
		"subq $0x38, %%rdi\n"
		"addq $0x08, %%rbx\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		: "+D"(prod), "+S"(mul1), "+d"(mul2) : 
		: "memory", "%rax", "%rbx", "%rcx", "%r8", "%r9", "cc"
	);
}

static void dfsrsa_fastmul32_x86_64(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"xorq %%rax, %%rax\n"
		"movl $0x10, %%ecx\n"
		"rep  stosq\n"
		"movq %%rdx, %%rbx\n"
		"subq $0x80, %%rdi\n"
		"movl $0x10, %%ecx\n"
	     "1: movq (%%rbx), %%r8\n"
		"xor  %%r9, %%r9\n"
		FASTMUL32_X86_64
		"movq %%r9, (%%rdi)\n"
		"subq $0x80, %%rsi\n"
		"subq $0x78, %%rdi\n"
		"addq $0x08, %%rbx\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		: "+D"(prod), "+S"(mul1), "+d"(mul2) : 
		: "memory", "%rax", "%rbx", "%rcx", "%r8", "%r9", "cc"
	);
}

#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 64
static void dfsrsa_fastmul64_x86_64(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"xorq %%rax, %%rax\n"
		"movl $0x20, %%ecx\n"
		"rep  stosq\n"
		"movq %%rdx, %%rbx\n"
		"subq $0x100, %%rdi\n"
		"movl $0x20, %%ecx\n"
	     "1: movq (%%rbx), %%r8\n"
		"xor  %%r9, %%r9\n"
		FASTMUL64_X86_64
		"movq %%r9, (%%rdi)\n"
		"subq $0x100, %%rsi\n"
		"subq $0xF8, %%rdi\n"
		"addq $0x08, %%rbx\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		: "+D"(prod), "+S"(mul1), "+d"(mul2) : 
		: "memory", "%rax", "%rbx", "%rcx", "%r8", "%r9", "cc"
	);
}
#endif

#endif

static void dfsrsa_fastmul_x86_64(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2, int len) {
#ifdef USE_FAST_MULT_FIXED
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 64
	if (len == 64) { dfsrsa_fastmul64_x86_64(prod, mul1, mul2); return; }
#endif
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 32
	if (len == 32) { dfsrsa_fastmul32_x86_64(prod, mul1, mul2); return; }
#endif
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 16
	if (len == 16) { dfsrsa_fastmul16_x86_64(prod, mul1, mul2); return; }
#endif
#endif
	if (len & 1){ dfsrsa_mul(prod, mul1, mul2, len); return; }
	asm volatile(
		"shrl $1, %%ecx\n"
		"xorq %%rax, %%rax\n"
		"movq %%rcx, %%r10\n"
		"movq %%rcx, %%r11\n"
		"leaq (%%rax,%%rcx,8), %%r12\n"
		"rep  stosq\n"
		"movq %%r10, %%rcx\n"
		"subq %%r12, %%rdi\n"
		"movq %%rdx, %%rbx\n"
	     "1: movq (%%rbx), %%r8\n"
		"xor  %%r9, %%r9\n"
	     "2: \n"
		FASTMUL2_X86_64
		"loop 2b\n"
		"movq %%r9, (%%rdi)\n"
		"movq %%r11, %%rcx\n"
		"subq %%r12, %%rdi\n"
		"subq %%r12, %%rsi\n"
		"addq $0x8, %%rbx\n"
		"addq $0x8, %%rdi\n"
		"decq %%r10\n"
		"jnz  1b\n"
		: "+D"(prod), "+S"(mul1), "+d"(mul2), "+c"(len) : 
		: "memory", "%rax", "%rbx", "%r8", "%r9", "%r10", "%r11", "%r12", "cc"
	);
}

#define dfsrsa_mul dfsrsa_fastmul_x86_64
#endif

#if defined(USE_FAST_MULT) && defined(__GNUC__) && defined(__arm__)

#if defined(USE_FAST_MULT_FIXED)

#if defined(__ARCH_ARM_6__) || defined(__ARCH_ARM_7__)
#define LDRD( r1,r2,raddr)	"ldrd " #r1 ", [" #raddr "]\n"
#define LDRDI(r1,r2,raddr)	"ldrd " #r1 ", [" #raddr "], #8\n"
#define STRD( r1,r2,raddr)	"strd " #r1 ", [" #raddr "]\n"
#define STRDI(r1,r2,raddr)	"strd " #r1 ", [" #raddr "], #8\n"
#else
#define LDRD( r1,r2,raddr)	"ldmia " #raddr  ", {" #r1 ", " #r2 "}\n"
#define LDRDI(r1,r2,raddr)	"ldmia " #raddr "!, {" #r1 ", " #r2 "}\n"
#define STRD( r1,r2,raddr)	"stmia " #raddr  ", {" #r1 ", " #r2 "}\n"
#define STRDI(r1,r2,raddr)	"stmia " #raddr "!, {" #r1 ", " #r2 "}\n"
#endif

#define SETMEM2_ARM	STRDI(r6, r7, r4)

#define SETMEM4_ARM	SETMEM2_ARM  SETMEM2_ARM
#define SETMEM8_ARM	SETMEM4_ARM  SETMEM4_ARM
#define SETMEM16_ARM	SETMEM8_ARM  SETMEM8_ARM
#define SETMEM32_ARM	SETMEM16_ARM SETMEM16_ARM
#define SETMEM64_ARM	SETMEM32_ARM SETMEM32_ARM

#define FASTMUL2_ARM	 			\
		"mov   r12, #0\n"		\
		 LDRDI (r8, r9, r2)		\
		"mov   r10, #0\n"		\
		"umlal r6,  r12, r8, r4\n"	\
		"umlal r7,  r10, r9, r4\n"	\
		"mov   r14, #0\n"		\
		"mov   r11, #0\n"		\
		"umlal r12, r14, r8, r5\n"	\
		"umlal r10, r11, r9, r5\n"	\
		"adds  r7,  r7,  r12\n"		\
		 LDRD  (r8, r9, r0)		\
		"adcs  r10, r10, r14\n"		\
	        "adc   r11, r11, #0\n"		\
		"adds  r8, r6, r8\n"		\
		"adcs  r9, r7, r9\n"		\
		"adcs  r6, r10, #0\n"		\
		 STRDI (r8, r9, r0)		\
	        "adc   r7, r11, #0\n"

#define FASTMUL4_ARM	FASTMUL2_ARM  FASTMUL2_ARM
#define FASTMUL8_ARM	FASTMUL4_ARM  FASTMUL4_ARM
#define FASTMUL16_ARM	FASTMUL8_ARM  FASTMUL8_ARM
#define FASTMUL32_ARM	FASTMUL16_ARM FASTMUL16_ARM
#define FASTMUL64_ARM	FASTMUL32_ARM FASTMUL32_ARM

static void dfsrsa_fastmul16_arm(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"stmfd sp!, {r0-r12, r14}\n"
		"mov  r0, %0\n"
		"mov  r1, %1\n"
		"mov  r2, %2\n"
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		"mov  r4, r0\n"
		SETMEM16_ARM
		"mov  r3, #8\n"
	     "1:\n"
		 LDRDI(r4, r5, r1)
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		FASTMUL16_ARM
		 STRDI(r6, r7, r0)
		"sub  r0, r0, #64\n"
		"sub  r2, r2, #64\n"
		"subs r3, r3, #1\n"
		"bne  1b\n"
		"ldmfd sp!, {r0-r12, r14}\n"
		: : "r"(prod), "r"(mul1), "r"(mul2)
		: "memory", "cc"
	);
}

static void dfsrsa_fastmul32_arm(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"stmfd sp!, {r0-r12, r14}\n"
		"mov  r0, %0\n"
		"mov  r1, %1\n"
		"mov  r2, %2\n"
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		"mov  r4, r0\n"
		SETMEM32_ARM
		"mov  r3, #16\n"
	     "1:\n"
		 LDRDI(r4, r5, r1)
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		FASTMUL32_ARM
		 STRDI(r6, r7, r0)
		"sub  r0, r0, #128\n"
		"sub  r2, r2, #128\n"
		"subs r3, r3, #1\n"
		"bne  1b\n"
		"ldmfd sp!, {r0-r12, r14}\n"
		: : "r"(prod), "r"(mul1), "r"(mul2)
		: "memory", "cc"
	);
}

static void dfsrsa_fastmul64_arm(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2) {
	asm volatile(
		"stmfd sp!, {r0-r12, r14}\n"
		"mov  r0, %0\n"
		"mov  r1, %1\n"
		"mov  r2, %2\n"
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		"mov  r4, r0\n"
		SETMEM64_ARM
		"mov  r3, #32\n"
	     "1:\n"
		 LDRDI(r4, r5, r1)
		"mov  r6, #0\n"
		"mov  r7, #0\n"
		FASTMUL64_ARM
		 STRDI(r6, r7, r0)
		"sub  r0, r0, #256\n"
		"sub  r2, r2, #256\n"
		"subs r3, r3, #1\n"
		"bne  1b\n"
		"ldmfd sp!, {r0-r12, r14}\n"
		: : "r"(prod), "r"(mul1), "r"(mul2)
		: "memory", "cc"
	);
}

#endif

static void dfsrsa_fastmul_arm(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2, int len) {
#ifdef USE_FAST_MULT_FIXED
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 64
	if (len == 64) { dfsrsa_fastmul64_arm(prod, mul1, mul2); return; }
#endif
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 32
	if (len == 32) { dfsrsa_fastmul32_arm(prod, mul1, mul2); return; }
#endif
#if !defined(USE_KARATSUBA) || KARATSUBA_TRESHOLD > 16
	if (len == 16) { dfsrsa_fastmul16_arm(prod, mul1, mul2); return; }
#endif
#endif
	asm volatile(
		"stmfd sp!, {r0-r9}\n"
		"mov  r0, %0\n"
		"mov  r1, %1\n"
		"mov  r2, %2\n"
		"mov  r3, %3\n"
		"mov  r4, #0\n"
		"mov  r5, r0\n"
		"mov  r6, r3\n"
	     "1: str  r4, [r5], #4\n"
		"subs r6, r6, #1\n"
		"bne  1b\n"
		"mov  r5, r3\n"
	     "2: mov  r6, r3\n"
		"ldr  r4, [r1], #4\n"
		"mov  r7, #0\n"
	     "3: ldr  r8, [r2], #4\n"
		"mov  r9, #0\n"
		"umlal r7, r9, r4, r8\n"
		"ldr  r8, [r0]\n"
		"adds r7, r7, r8\n"
		"adc  r9, r9, #0\n"
	        "str  r7, [r0], #4\n"
		"mov  r7, r9\n"
		"subs r6, r6, #1\n"
		"bne  3b\n"
		"str  r7, [r0], #4\n"
		"sub  r0, r0, r3, LSL #2\n"
		"sub  r2, r2, r3, LSL #2\n"
		"subs r5, r5, #1\n"
		"bne  2b\n"
		"ldmfd sp!, {r0-r9}\n"
		: : "r"(prod), "r"(mul1), "r"(mul2), "r"(len)
		: "memory", "cc"
	);
}

#define dfsrsa_mul dfsrsa_fastmul_arm
#endif

#ifdef USE_KARATSUBA

/* метод Карацубы умножения чисел, work - вспомогательный массив размера 2 * len */
static void dfsrsa_karatsuba_mul(dfsrsa_t *prod, dfsrsa_t *mul1, dfsrsa_t *mul2,
		int len, dfsrsa_t *work) {
#ifdef DFSRSA_DEBUG
	dfsrsa_t *m1 = mul1, *m2 = mul2, *p = prod, *w = work; int l = len;
#endif
	int r, llen; dfsrsa_long_t rem;
	if ((len & 1) || len < KARATSUBA_TRESHOLD) {
	    dfsrsa_mul(prod, mul1, mul2, len);
	    return;
	}
	r = 1, llen = len, len >>= 1;
	if (dfsrsa_cmp(mul1 + len, mul1, len) >= 0) {
	    dfsrsa_sub(work, mul1 + len, mul1, len);
	} else {
	    dfsrsa_sub(work, mul1, mul1 + len, len), r = -r;
	}
	if (dfsrsa_cmp(mul2 + len, mul2, len) >= 0) {
	    dfsrsa_sub(work + len, mul2 + len, mul2, len);
	} else {
	    dfsrsa_sub(work + len, mul2, mul2 + len, len), r = -r;
	}
	dfsrsa_karatsuba_mul(work + llen, work, work + len, len, prod);
	dfsrsa_karatsuba_mul(prod, mul1, mul2, len, work);
	dfsrsa_karatsuba_mul(prod + llen, mul1 + len, mul2 + len, len, work);
	dfsrsa_copy(work, prod, llen);
	mul1 = prod + llen, mul2 = work, work += llen, prod += len, rem = 0;
	if (r > 0) {
	    while (llen--) {
		rem += (dfsrsa_long_t)*prod + (dfsrsa_long_t)*mul1++
			+ (dfsrsa_long_t)*mul2++ - (dfsrsa_long_t)*work++;
		*prod++ = (dfsrsa_t)rem;
		rem = (dfsrsa_slong_t)rem >> DFSRSA_T_BITS;
	    }
	} else {
	    while (llen--) {
		rem += (dfsrsa_long_t)*prod + (dfsrsa_long_t)*mul1++
			+ (dfsrsa_long_t)*mul2++ + (dfsrsa_long_t)*work++;
		*prod++ = (dfsrsa_t)rem;
		rem >>= DFSRSA_T_BITS;
	    }
	}
	while (rem) {
	    rem += *prod, *prod++ = (dfsrsa_t)rem, rem >>= DFSRSA_T_BITS;
	}
#ifdef DFSRSA_DEBUG
	dfsrsa_mul(w, m1, m2, l);
	if (dfsrsa_cmp(p, w, l<<1)) {
	    dfsrsa_debug(KARATSUBA, "mul1", m1, l);
	    dfsrsa_debug(KARATSUBA, "mul2", m2, l);
	    dfsrsa_debug(KARATSUBA, "prod0", w, l<<1);
	    dfsrsa_debug(KARATSUBA, "prod", p, l<<1);
	}
#endif
}

#ifdef dfsrsa_mul
#undef dfsrsa_mul
#endif
#define dfsrsa_mul(prod,mul1,mul2,len) dfsrsa_karatsuba_mul(prod,mul1,mul2,len,kwork)

#endif



/******************************
 * мультипликативные операции *
 ******************************/



/* вычисляет остаток от деления числа num длины len на короткое число mod
*/
static dfsrsa_t dfsrsa_modsmall(dfsrsa_t *div, int len, dfsrsa_t mod){
	dfsrsa_long_t rem = 0;
	if (!mod) return 0;
	while (len) {
	    rem <<= DFSRSA_T_BITS, rem |= div[--len], rem %= mod;
	}
	return rem;
}

/* вычисляет остаток от деления числа mod длины mlen на число div длины len,
   результат помещает в массив mod; возвращает -1 при делении на 0
*/
static int dfsrsa_mod(dfsrsa_t *mod, int mlen, dfsrsa_t *div, int len){
	int res, offset; dfsrsa_long_t mod0, div0, div1, div2;
	dfsrsa_debug(MOD, "divisor", mod, mlen);
	dfsrsa_debug(MOD, "dividend", div, len);
	len = dfsrsa_getlen(div, len);
	if (!len) return -1;
	div1 = div[len - 1];
	if (len > 1){
	    div2 = (div1 << DFSRSA_T_BITS) | div[len - 2];
	    ++div1;
	    if (len > 2) ++div2;
	} else div2 = 0;
	for (;;) {
	    mlen = dfsrsa_getlen(mod, mlen);
	    dfsrsa_debug(MODLOW, "work remainder", mod, mlen);
	    offset = mlen - len;
	    if (offset < 0) break;
	    res = dfsrsa_cmp(mod + offset, div, len);
	    mod0 = (dfsrsa_long_t)mod[mlen - 1];
	    if (res < 0) {
			if (!offset--) break;
			mod0 = (mod0 << DFSRSA_T_BITS) | mod[mlen - 2];
			div0 = div1;
	    } else if (len > 1 && div2) {
			mod0 = (mod0 << DFSRSA_T_BITS) | mod[mlen - 2];
			div0 = div2;
	    } else {
			div0 = div1;
	    }
	    dfsrsa_debug(MODLOW, "mod0", (dfsrsa_t*)&mod0, 2);
	    dfsrsa_debug(MODLOW, "div0", (dfsrsa_t*)&div0, 2);
	    mod0 /= div0;
	    if (!mod0) mod0 = 1;
	    dfsrsa_debug(MODLOW, "mod0/div0", (dfsrsa_t*)&mod0, 2);
	    dfsrsa_mulsub(mod + offset, div, (dfsrsa_t)mod0, len);
	}
	dfsrsa_debug(MOD, "remainder", mod, mlen);
	return 0;
}

/* вычисляет частное и остаток от деления числа mod длины mlen на число div длины len,
   частное помещает в массив quotient длины mlen, остаток - в массив mod;
   возвращает -1 при делении на 0
*/
int dfsrsa_divmod(dfsrsa_t *mod, int mlen, dfsrsa_t *div, int len,
		dfsrsa_t *quotient){
	int res, offset; dfsrsa_long_t mod0, div0, div1, div2;
	dfsrsa_debug(MOD, "divisor", mod, mlen);
	dfsrsa_debug(MOD, "dividend", div, len);
	len = dfsrsa_getlen(div, len);
	if (!len) return -1;
	dfsrsa_set(quotient, mlen, 0);
	div1 = div[len - 1];
	if (len > 1){
	    div2 = (div1 << DFSRSA_T_BITS) | div[len - 2];
	    ++div1;
	    if (len > 2) ++div2;
	} else div2 = 0;
	for (;;) {
	    mlen = dfsrsa_getlen(mod, mlen);
	    dfsrsa_debug(MODLOW, "work remainder", mod, mlen);
	    offset = mlen - len;
	    if (offset < 0) break;
	    res = dfsrsa_cmp(mod + offset, div, len);
	    mod0 = (dfsrsa_long_t)mod[mlen - 1];
	    if (res < 0) {
			if (!offset--) break;
			mod0 = (mod0 << DFSRSA_T_BITS) | mod[mlen - 2];
			div0 = div1;
	    } else if (len > 1 && div2) {
			mod0 = (mod0 << DFSRSA_T_BITS) | mod[mlen - 2];
			div0 = div2;
	    } else {
			div0 = div1;
	    }
	    dfsrsa_debug(MODLOW, "mod0", (dfsrsa_t*)&mod0, 2);
	    dfsrsa_debug(MODLOW, "div0", (dfsrsa_t*)&div0, 2);
	    mod0 /= div0;
	    if (!mod0) mod0 = 1;
	    dfsrsa_debug(MODLOW, "mod0/div0", (dfsrsa_t*)&mod0, 2);
	    dfsrsa_mulsub(mod + offset, div, (dfsrsa_t)mod0, len);
	    dfsrsa_addeq(quotient + offset, (dfsrsa_t)mod0, mlen - offset);
	}
	dfsrsa_debug(MOD, "quotient", quotient, mlen);
	dfsrsa_debug(MOD, "remainder", mod, mlen);
	return 0;
}

/* возводит в степень по модулю: res = base^exp (mod mod), все числа длины len;
   work - вспомогательный массив длины 6*len, куда записываются два последних
   результата умножения, каждый длины 2*len
*/
static void dfsrsa_powmod(dfsrsa_t *res, dfsrsa_t *base, dfsrsa_t *exp, dfsrsa_t *mod,
		dfsrsa_t *work, int len){
#define kwork (work + 4 * len)
	dfsrsa_t bits; int explen, off = 0, off1 = len << 1, flag = 0, i;
	dfsrsa_set(work, len, 1);
	explen = dfsrsa_getlen(exp, len);
	while (explen--) {
	    bits = exp[explen], i = DFSRSA_T_BITS;
	    while (i--) {
		if (flag) {
		    off = off1 - off;
		    dfsrsa_mul(work + off, work + off1 - off, work + off1 - off, len);
		    dfsrsa_mod(work + off, len << 1, mod, len);
		}
		if ((bits >> i) & 1) {
		    off = off1 - off;
		    dfsrsa_mul(work + off, work + off1 - off, base, len);
		    dfsrsa_mod(work + off, len << 1, mod, len);
		    flag = 1;
		}
	    }
	}
	dfsrsa_copy(res, work + off, len);
#undef kwork
}

/* находит обратное число inv к данному num по модулю mod, если нет, возвращает -1;
   используется вариант расширенного бинарного алгоритма Евклида;
   считается, что num = 2k + 1, mod = 2^n (2m + 1), n <= 2;
   work - рабочий массив длины 4*len
*/ 
static int dfsrsa_inverse(dfsrsa_t *inv, dfsrsa_t *num, dfsrsa_t *mod, 
		dfsrsa_t *work, int len){
	dfsrsa_t *pn = work, *px = inv, *qn = work + len, *qx = work + 2*len,
		*mod4 = work + 3*len, mask = 0;
	int carry;
	dfsrsa_copy(pn, num, len), dfsrsa_set(px, len, 1);
	dfsrsa_copy(mod4, mod,  len);
	while (!(*mod4 & 1)) dfsrsa_shr1(mod4, len, 0), mask <<= 1, mask |= 1;
	dfsrsa_copy(qn, mod4, len), dfsrsa_set(qx, len, 0);
	dfsrsa_debug(INV, "inv mod/4", mod4, len); 
	for(;;){
	    dfsrsa_debug(INV, "inv pn", pn, len); 
	    dfsrsa_debug(INV, "inv px", px, len); 
	    dfsrsa_debug(INV, "inv qn", qn, len); 
	    dfsrsa_debug(INV, "inv qx", qx, len); 
	    while (!(*pn & 1)) {
		carry = 0;
		dfsrsa_shr1(pn, len, 0);
		if (*px & 1) carry = dfsrsa_add(px, px, mod4, len);
		dfsrsa_shr1(px, len, carry);
	    }
	    while (!(*qn & 1)) {
		carry = 0;
		dfsrsa_shr1(qn, len, 0);
		if (*qx & 1) carry = dfsrsa_add(qx, qx, mod4, len);
		dfsrsa_shr1(qx, len, carry);
	    }
	    dfsrsa_debug(INV, "inv pn", pn, len); 
	    dfsrsa_debug(INV, "inv px", px, len); 
	    dfsrsa_debug(INV, "inv qn", qn, len); 
	    dfsrsa_debug(INV, "inv qx", qx, len); 
	    switch(dfsrsa_cmp(pn, qn, len)) {
		case 0: goto fin;
		case 1:
		    dfsrsa_sub(pn, pn, qn, len);
		    if (dfsrsa_sub(px, px, qx, len))
			dfsrsa_add(px, px, mod4, len);
		    break;
		case -1:
		    dfsrsa_sub(qn, qn, pn, len);
		    if (dfsrsa_sub(qx, qx, px, len))
			dfsrsa_add(qx, qx, mod4, len);
		    break;
	    }
	}
fin:
	dfsrsa_set(qn, len, 1);
	if (dfsrsa_cmp(pn, qn, len)) return -1;
	dfsrsa_debug(INV, "inv mod/4", inv, len);
	while ((*num ^ *inv) & mask) dfsrsa_add(inv, inv, mod4, len);
	dfsrsa_debug(INV, "inv result", inv, len); 
	return 0;
}



/********************
 * метод Монтгомери *
 ********************/



/* по данному модулю mod длины len
   инициализирует массив данных mdata длины 6*len для метода Монтгомери,
   используя вспомогательный массив work длины 4*len;
   в случае неудачи возвращает -1;
*/
static int dfsrsa_montgomery_init(dfsrsa_t *mod, dfsrsa_t *mdata, dfsrsa_t *work,
		int len) {
	dfsrsa_debug(MONT, "montgomery N", mod, len);
	dfsrsa_set(mdata + 2 * len, len, 0);
	dfsrsa_sub(mdata, mdata + 2 * len, mod, len);
	//dfsrsa_mod(mdata, len, mod, len);
	dfsrsa_debug(MONT, "montgomery R - N", mdata, len);
	if (dfsrsa_inverse(mdata + 3 * len, mdata, mod, work, len)) return -1;
	dfsrsa_debug(MONT, "montgomery R^-1", mdata + 3 * len, len);
	if (dfsrsa_divmod(mdata + 2 * len, 2 * len, mod, len, mdata)) return -1;
	dfsrsa_debug(MONT, "montgomery k", mdata, len);
	return 0;
}

/* редуцирует данное число num для использования в методе Монтгомери:
   num = (num << R) mod mod, num и mod имеют длину len, R = len * 8 * sizeof(dfsrsa_t)
   mdata - массив, инициализированный функцией dfsrsa_montgomery_init;
   в случае ошибки возвращает -1;
*/
static int dfsrsa_montgomery_reduce(dfsrsa_t *num, dfsrsa_t *mod, dfsrsa_t *mdata,
		int len) {
	dfsrsa_debug(MONT, "montgomery n", num, len);
	dfsrsa_set(mdata + 2 * len, len, 0);
	dfsrsa_copy(mdata + 3 * len, num, len);
	if (dfsrsa_mod(mdata + 2 * len, 2 * len, mod, len)) return -1;
	dfsrsa_copy(num, mdata + 2 * len, len);
	dfsrsa_debug(MONT, "montgomery nR", num, len);
	return 0;	
}

/* шаг метода Монтгомери: приводит данное число num по модулю mod, деля на R,
   num имеет длину 2 * len, mod - len, в результат (num) имеет длину len;
   mdata - массив, инициализированный функцией dfsrsa_montgomery_init
*/
static void dfsrsa_montgomery_mod(dfsrsa_t *num, dfsrsa_t *mod, dfsrsa_t *mdata, int len) {
#define kwork (mdata + 4 * len)
	dfsrsa_debug(MONT, "montgomery n", num, 2*len);
	if (dfsrsa_getlen(num, len)) {
	    dfsrsa_mul(mdata + len, num, mdata, len);
	    dfsrsa_mul(mdata + 2 * len, mdata + len, mod, len);
	    if (dfsrsa_adc(num, num + len, mdata + 3 * len, 1, len)
		    || dfsrsa_cmp(num, mod, len) >= 0) {
	        dfsrsa_sub(num, num, mod, len);
	    }
	} else {
	    dfsrsa_copy(num, num + len, len);
	    if (dfsrsa_cmp(num, mod, len) >= 0) {
	        dfsrsa_sub(num, num, mod, len);
	    }
	}
	dfsrsa_debug(MONT, "montgomery n/R", num, len);
#undef kwork
}

/* возводит в степень по модулю, используя метод Монтгомери: 
   res = base^exp (mod mod), все числа длины len;
   work - вспомогательный массив длины 12*len, в начало которого записываются
   два последних результата умножения, каждый длины 2*len
*/
static void dfsrsa_montgomery_powmod(dfsrsa_t *res, dfsrsa_t *base, dfsrsa_t *exp, 
		dfsrsa_t *mod, dfsrsa_t *work, int len){
#define kwork (work + 8 * len)
	dfsrsa_t bits, *mdata = work + 4 * len, *mbase = work + 10 * len;
	int explen, off = 0, off1 = len << 1, flag = 0, i;
	if (dfsrsa_montgomery_init(mod, mdata, work, len)) goto powmod;
	dfsrsa_set(work, len, 1);
	if (dfsrsa_montgomery_reduce(work, mod, mdata, len)) goto powmod;
	dfsrsa_copy(mbase, base, len);
	if (dfsrsa_montgomery_reduce(mbase, mod, mdata, len)) goto powmod;
	explen = dfsrsa_getlen(exp, len);
	while (explen--) {
	    bits = exp[explen], i = DFSRSA_T_BITS;
	    while (i--) {
		if (flag) {
		    off = off1 - off;
		    dfsrsa_mul(work + off, work + off1 - off, work + off1 - off, len);
		    dfsrsa_montgomery_mod(work + off, mod, mdata, len);
		}
		if ((bits >> i) & 1) {
		    off = off1 - off;
		    dfsrsa_mul(work + off, work + off1 - off, mbase, len);
		    dfsrsa_montgomery_mod(work + off, mod, mdata, len);
		    flag = 1;
		}
	    }
	}
	dfsrsa_set(work + len, len, 0);
	dfsrsa_montgomery_mod(work       , mod, mdata, len);
	dfsrsa_set(work + off1 + len, len, 0);
	dfsrsa_montgomery_mod(work + off1, mod, mdata, len);
	dfsrsa_copy(res, work + off, len);
	return;
powmod: perror("Montgomery_powmod failed! Using std powmod...");
	dfsrsa_powmod(res, base, exp, mod, work, len);
#undef kwork
}

#ifdef USE_MONTGOMERY
#define dfsrsa_powmod dfsrsa_montgomery_powmod
#endif



/******************************
 * теоретико-числовые функции *
 ******************************/


/* простейший тест простоты для маленьких чисел
*/
static int dfsrsa_isprime0(dfsrsa_t n){
	dfsrsa_t d;
	if (n < 32) return (0xA08A28AC >> n) & 1;
	if (!(n & 1)) return 0;
	for (d = 3; d * d <= n; d += 2) {
	    if (!(n % d)) return 0;
	}
	return 1;
}

/* тест простоты, основанный на детерминированном алгоритме Миллера (в предположении
   гипотезы Римана), работает только для чисел вида 4k + 3;
   work - рабочий массив длиной 16*len;
   алгоритм: для каждого простого a = 2,...,lim проверяется, что:
	a ^ (n-1) = 1 (mod n)
   и
	a ^ ((n-1)/2) = 1 или -1 (mod n)
   где
	lim = (log_2 (n)) ^ 2
   (по результату Eric Bach 1985 достаточно проверять до 2 ln^2(n), а это составляет
   примерно 0,96 от нашего lim, который считать удобнее)
*/
static int dfsrsa_isprime(dfsrsa_t *n, dfsrsa_t *work, int len){
	dfsrsa_t *work2, *res, *a, *nm1, *o, lim, d, i;
	dfsrsa_debug(MILLER, "miller n", n, len);
	len = dfsrsa_getlen(n, len);
	lim = (dfsrsa_t)len * DFSRSA_T_BITS;
	for (i = 0, d = 3; i < lim; d += 2) {
	    if (len <= 1 && d * d > *n) return 1;
	    if (!dfsrsa_isprime0(d)) continue;
	    if (!dfsrsa_modsmall(n, len, d)) return 0;
	    ++i;
	}
	work2 = work + 2 * len, res = work + 12 * len, a = res + len, 
	nm1 = a + len, o = nm1 + len;
	dfsrsa_set(o, len, 1); dfsrsa_set(a, len, 0);
	dfsrsa_copy(nm1, n, len), --*nm1;
	dfsrsa_debug(MILLER, "miller n-1", nm1, len);
#ifndef USE_MILLER_RABIN
	lim *= lim;
#endif
	if (len <= 1 && lim > *n) lim = *n;
#ifdef USE_MILLER_RABIN
	for (i = 0, d = 2; i < lim; ++d, d |= 1)
#else
	for (d = 2; d < lim; ++d, d |= 1)
#endif
	  if (dfsrsa_isprime0(d)) {
	    *a = d;
#ifdef USE_MILLER_RABIN
	    ++i;
#endif
	    dfsrsa_debug(MILLER, "miller a", a, len);
	    dfsrsa_powmod(res, a, nm1, n, work, len);
	    dfsrsa_debug(MILLER, "miller a^(n-1)", res, len);
	    dfsrsa_debug(MILLER, "miller work", work, len);
	    dfsrsa_debug(MILLER, "miller work2", work2, len);
	    /* здесь предполагается, что два последних результата возведения
	       в степень содержатся в work и work2, неважно, в каком порядке,
	       а это и будут a^((n-1)/2) и a^(n-1)
	    */
	    if ((dfsrsa_cmp(work,  o, len) && dfsrsa_cmp(work,  nm1, len))
	     || (dfsrsa_cmp(work2, o, len) && dfsrsa_cmp(work2, nm1, len)))
		return 0;
	}
	return 1;
}

/* генерирует простое число заданной длины на основе случайной информации в массиве n;
   work - рабочий массив размера 16*len
*/
static void dfsrsa_genprime(dfsrsa_t *n, dfsrsa_t *work, int len){
	dfsrsa_debug(GEN, "genprime random", n, len);
	*n |= 3, n[len - 1] |= 3 << (DFSRSA_T_BITS - 2);
	dfsrsa_debug(GEN, "genprime before", n, len);
	while(!dfsrsa_isprime(n, work, len)){
	    dfsrsa_addeq(n, 4, len);
	}
	dfsrsa_debug(GEN, "genprime result", n, len);
}

/* генерирует взаимно-обратные числа pubkey и privkey по данному модулю mod вида 8k+4,
   на входе pubkey заполнен случайными данными, work - рабочий массив длины 4*len
*/ 
static void dfsrsa_geninv(dfsrsa_t *privkey, dfsrsa_t *pubkey, dfsrsa_t *mod,
		dfsrsa_t *work, int len){
	dfsrsa_debug(GEN, "e random", pubkey, len); 
	dfsrsa_mod(pubkey, len, mod, len);
	dfsrsa_debug(GEN, "e mod", pubkey, len); 
	*pubkey |= 1;
	dfsrsa_debug(GEN, "e before", pubkey, len); 
	while (dfsrsa_inverse(privkey, pubkey, mod, work, len)) {
	    dfsrsa_addeq(pubkey, 2, len);
	}
	dfsrsa_debug(GEN, "e result", pubkey, len); 
	dfsrsa_debug(GEN, "d result", privkey, len); 
}



/********************
 * основные функции *
 ********************/


/* выделяет число данной длины
*/
static dfsrsa_t *dfsrsa_alloc(int len){
	return (dfsrsa_t *)malloc(len * sizeof(dfsrsa_t));
}

/* освобождает число
*/
static void dfsrsa_free(dfsrsa_t *num){
	free(num);
}

/* закодировать/раскодировать сообщение, используя соответствующий открытый/закрытый
   ключ, datalen и keylen измеряются в числах dfsrsa_t, ключ key должен быть ранее
   сгенерирован функцией dfsrsa_keygen, datalen должно быть кратно половине от keylen;
   в каждой порции сообщения (длиной keylen/2 чисел dfsrsa_t) старший бит должен быть 0;
   итоговое сообщение помещается в тот же массив data, что и исходное;
   при ошибке возвращает -1
*/
int dfsrsa_crypt(dfsrsa_t *data, int datalen, dfsrsa_t *key, int keylen){
	int len = keylen / 2, i;
	dfsrsa_t *work, *mod = key + len;
	if (keylen <= 0 || (keylen & 1) || datalen < 0 || datalen % len) return -1;
	datalen /= len;
	for (i = 0; i < datalen; ++i) {
	    if (dfsrsa_cmp(data + i * len, mod, len) >= 0) return -1;
	}
	work = dfsrsa_alloc(len << 4);
	if (!work) return -1;
	for (i = 0; i < datalen; ++i, data += len) {
	    dfsrsa_powmod(data, data, key, mod, work, len);
	}
	dfsrsa_free(work);
	return 0;
}

/* сгенерировать открытый и закрытый ключи длины keylen чисел dfsrsa_t, keylen должно
   быть кратно 4, массив pubkey должен быть предварительно заполнен случайными
   числами, алгоритм не использует никакой другой случайной информации;
   при ошибке возвращает -1
*/
int dfsrsa_keygen(dfsrsa_t *privkey, dfsrsa_t *pubkey, int keylen){
#define kwork work
	int len = keylen >> 2;
	dfsrsa_t *work, *p = pubkey + len * 2, *q = pubkey + len * 3,
		*n = privkey + len * 2, *phin = p, *phin0 = privkey;
	if (keylen <= 0 || (keylen & 3)) return -1;
	if ((dfsrsa_t)len * DFSRSA_T_BITS > (dfsrsa_t)1 << (DFSRSA_T_BITS >> 1))
	    return -1;
	work = dfsrsa_alloc(len << 4);
	if (!work) return -1;
	dfsrsa_genprime(p, work, len);
	dfsrsa_genprime(q, work, len);
	dfsrsa_mul(n, p, q, len);
	dfsrsa_debug(GEN, "pq", n, len << 1);
	--*p, --*q;
	dfsrsa_mul(phin0, p, q, len);
	len <<= 1;
	dfsrsa_copy(phin, phin0, len);
	dfsrsa_debug(GEN, "(p-1)(q-1)", phin, len);
	dfsrsa_geninv(privkey, pubkey, phin, work, len);
	dfsrsa_copy(phin, n, len);
	dfsrsa_free(work);
	return 0;
#undef kwork
}



/****************
 * тестирование *
 ****************/

#if !defined(DFSTOOLS) && !defined(__DuS__) && !defined(QDNET)
#define DFSRSA_TEST
#endif

#ifdef DFSRSA_TEST

#include <limits.h>
#include <unistd.h>
#include <sys/times.h>
#include "../dfslib/dfslib_random.h"

static void dfsrsa_fillrand(dfsrsa_t *arr, int len){
	dfslib_random_fill(arr, len * (DFSRSA_T_BITS >> 3), 0, 0);
}

#define MINLEN		4
#define MAXLEN		(TEST_MAX_BITS >> 4)
#define MAXDATALEN	(TEST_MAX_BITS >> 3)
#define STARTLEN	4
#define ENDLEN  	4

int work(int keylen, int datalen){
	dfsrsa_t pubkey[MAXLEN], privkey[MAXLEN], data[MAXDATALEN], data0[MAXDATALEN];
	int i;
	if (keylen > MAXLEN) return -1;
	if (datalen > MAXDATALEN) return -2;

	/* генерация ключей */
	dfsrsa_fillrand(pubkey, keylen);
	dfsrsa_debug(TOP, "pubkey random", pubkey, keylen);
	dfsrsa_set(privkey, keylen, 0);
	if (dfsrsa_keygen(privkey, pubkey, keylen)) return -3;
	dfsrsa_debug(TOP, "pubkey",  pubkey,  keylen);
	dfsrsa_debug(TOP, "privkey", privkey, keylen);

	/* генерация данных, старший бит в каждой порции данных должен быть 0 */
	dfsrsa_fillrand(data0, datalen);
	for (i = 1; i <= datalen/(keylen/2); ++i)
	    data0[i * keylen/2 - 1] &= ((dfsrsa_t)1 << (DFSRSA_T_BITS - 1)) - 1;
	dfsrsa_copy(data, data0, datalen);
	dfsrsa_debug(TOP, "data", data, datalen);

	/* кодирование открытым ключом, декодирование закрытым */
	if (dfsrsa_crypt(data, datalen, pubkey,  keylen)) return -4;
	dfsrsa_debug(TOP, "data+pub", data, datalen);
	if (!dfsrsa_cmp(data, data0, datalen)) return -5;
	if (dfsrsa_crypt(data, datalen, privkey, keylen)) return -6;
	dfsrsa_debug(TOP, "data+pub+priv", data, datalen);
	if (dfsrsa_cmp(data, data0, datalen)) return -7;

	/* кодирование закрытым ключом, декодирование открытым */
	if (dfsrsa_crypt(data, datalen, privkey, keylen)) return -8;
	dfsrsa_debug(TOP, "data+priv", data, datalen);
	if (!dfsrsa_cmp(data, data0, datalen)) return -9;
	if (dfsrsa_crypt(data, datalen, pubkey,  keylen)) return -10;
	dfsrsa_debug(TOP, "data+priv+pub", data, datalen);
	if (dfsrsa_cmp(data, data0, datalen)) return -11;

	return 0;
}

static void miller_test(void){
	dfsrsa_t n = 0xCB0DB667, work[16]; int i, res = 0, res1;
	for(i = 0; i <= 100 || !res; ++i, n += 4) {
	    res = dfsrsa_isprime(&n, work, 1);
	    res1 = dfsrsa_isprime0(n);
	    if (res != res1) {
		printf("Error: %u is %sprime, Miller wrong!\n", n, (res1 ? "" : "not "));
		exit(0);
	    }
	}
}

int main(int argc, char **argv){
	int len, keylen, datalen, res; struct tms t0, t;
	printf("%s %s %s\n", DFSRSA_TITLE, DFSRSA_VERSION, DFSRSA_COPYRIGHT);
	miller_test();
	for (keylen = MINLEN; keylen <= MAXLEN; keylen <<= 1) {
	    len = keylen/2;
	    for (datalen = STARTLEN*len; datalen <= ENDLEN*len; datalen += len) {
		printf("%s test: keylen = %4d bits, datalen = %5d bits, result: ",
			argv[0], len * DFSRSA_T_BITS, datalen * DFSRSA_T_BITS);
		fflush(stdout);
		times(&t0);
		res = work(keylen, datalen);
		times(&t);
		if (res) printf("error %d\n", res);
		else printf("OK, time = %8.4lf + %8.4lf s\n",
		    (double)(t.tms_utime - t0.tms_utime) / sysconf(_SC_CLK_TCK),
		    (double)(t.tms_stime - t0.tms_stime) / sysconf(_SC_CLK_TCK)
		);
		fflush(stdout);
	    }
	}
	return 0;
}

#endif
