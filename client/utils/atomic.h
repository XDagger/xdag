/* atomic, T14.618-T14.618 $DVS:time$ */

#ifndef _UTILS_ATOMIC_H
#define _UTILS_ATOMIC_H

#if defined(_WIN32) || defined(_WIN64)

/*******************  WINDOWS  *****************************
Standard C11 atomic functions can take any atomic type without regards of the size
so, for example, atomic_exchange can take atomic char or atomic uint64_t
but on windows _Interlocked functions are divided by size:
- _InterlockedExchange8 - _InterlockedExchange16 - _InterlockedExchange - _InterlockedExchange64 - _InterlockedExchange128.

So you can easily map the atomic function in the correct _Interlocked function, for example:

atomic_exchange_uint_least8(a,b) will be directly mapped to atomic_exchange(a,b) for C11 systems and
it will be directly implemented with _Interlocked on Windows in stdatomic.h or here:

example:

uint_least8_t atomic_exchange_uint_least8(atomic_uint_least8_t *object, uint_least8_t desired)
{
	switch(sizeof(uint_least8_t)){
		case 1:
			return _InterlockedExchange8((char volatile*)object, desired);
		case 2:
			return _InterlockedExchange16((short volatile*)object, desired);
		...continue...
}
**********************************************************/

typedef enum memory_order {
	memory_order_relaxed,
	memory_order_consume,
	memory_order_acquire,
	memory_order_release,
	memory_order_acq_rel,
	memory_order_seq_cst
} memory_order;

typedef volatile int atomic_int;
typedef volatile uintptr_t atomic_uintptr_t;
typedef volatile uint_least64_t atomic_uint_least64_t;

#define atomic_init_uintptr(a,b) atomic_init(a,b)
#define atomic_init_int(a,b) atomic_init(a,b)
#define atomic_init(ptr, value) (void)(*(ptr) = (value))

#define atomic_store_explicit_uintptr(a,b,c) atomic_store_explicit(a,b,c)
#define atomic_store_explicit_int(a,b,c) atomic_store_explicit(a,b,c)
#define atomic_store_explicit(a,b,c) atomic_store(a,b)
#define atomic_store(ptr, value) (void)(*(ptr) = (value))

#define atomic_load_explicit_uintptr(a,b) atomic_load_explicit(a,b)
#define atomic_load_explicit_int(a,b) atomic_load_explicit(a,b)
#define atomic_load_explicit(a,b) atomic_load(a)
#define atomic_load(ptr) *(ptr)

#define atomic_compare_exchange_strong_explicit_uintptr(a,b,c,d,e) atomic_compare_exchange_strong_uintptr(a,b,c)
#define atomic_compare_exchange_strong_explicit_uint_least64(a,b,c,d,e) atomic_compare_exchange_strong_uint_least64(a,b,c)
extern int atomic_compare_exchange_strong_uintptr(atomic_uintptr_t *ptr, uintptr_t *expected, uintptr_t desired);
extern int atomic_compare_exchange_strong_uint_least64(atomic_uint_least64_t *ptr, uint_least64_t *expected, uint_least64_t desired);

#define atomic_exchange_explicit_uint_least64(a,b,c) atomic_exchange_uint_least64(a,b)
extern uint_least64_t atomic_exchange_uint_least64(atomic_uint_least64_t *object, uint_least64_t desired);

#else

#include <stdatomic.h>

/*********************  C11  *************************************
Add here new atomic function when needed

#define atomic_functionName_atomicData(....)  atomic_functionName(....)
******************************************************************/

#define atomic_init_uintptr(a,b) atomic_init(a,b)
#define atomic_init_int(a,b) atomic_init(a,b)

#define atomic_store_explicit_uintptr(a,b,c) atomic_store_explicit(a,b,c)
#define atomic_store_explicit_int(a,b,c) atomic_store_explicit(a,b,c)

#define atomic_load_explicit_uintptr(a,b) atomic_load_explicit(a,b)
#define atomic_load_explicit_int(a,b) atomic_load_explicit(a,b)

#define atomic_compare_exchange_strong_explicit_uintptr(a,b,c,d,e) atomic_compare_exchange_strong_explicit(a,b,c,d,e)
#define atomic_compare_exchange_strong_explicit_uint_least64(a,b,c,d,e) atomic_compare_exchange_strong_explicit(a,b,c,d,e)

#define atomic_exchange_explicit_uint_least64(a,b,c) atomic_exchange_explicit(a,b,c)

#endif

#endif
