#ifndef _STDATOMIC_H
#define _STDATOMIC_H

#include <stdint.h>

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

#define atomic_init(ptr, value) (void)(*(ptr) = (value))

#define atomic_store(ptr, value) (void)(*(ptr) = (value))
#define atomic_store_explicit(ptr, value, memory_order) (void)(*(ptr) = (value))

#define atomic_load(ptr) *(ptr)
#define atomic_load_explicit(ptr, memory_order) *(ptr)

extern uint_least64_t atomic_exchange(atomic_uint_least64_t *object, uint_least64_t desired);
#define atomic_exchange_explicit(ptr, desired, memory_order) atomic_exchange(ptr, desired)

extern int atomic_compare_exchange_strong(atomic_int *ptr, int *expected, int desired);
#define atomic_compare_exchange_strong_explicit(ptr, expected, desired, memory_order_success, memory_order_failure) atomic_compare_exchange_strong(ptr, expected, desired)

#endif
