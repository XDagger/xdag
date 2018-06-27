/* атомарные операции, T11.596-T13.155; $DVS:time$ */

#ifndef	LDUS_ATOMIC_H_INCLUDED
#define LDUS_ATOMIC_H_INCLUDED

#include <stdint.h>

#define LDUS_USE_ATOMIC

#ifndef _GCC_VERSION
#define _GCC_VERSION (__GNUC__ * 10000 | __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if _GCC_VERSION <= 40603
#define __atomic_load_n(ptr, mode) (*(ptr))
#define __atomic_store_n(ptr, value, mode) (*(ptr) = (value))
#define __atomic_add_fetch(ptr, value, mode) __sync_add_and_fetch(ptr, value)
#define __atomic_sub_fetch(ptr, value, mode) __sync_sub_and_fetch(ptr, value)
#define __atomic_compare_exchange_n(ptr, old_, new_, param, mode1, mode2) (*(old_) = __sync_val_compare_and_swap(ptr, *(old_), new_))
#endif

typedef uint32_t ldus_atomic;
typedef uint64_t ldus_atomic64;

static inline uint32_t ldus_atomic_read(volatile ldus_atomic *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#else
	return *ptr;
#endif
}

static inline uint32_t ldus_atomic_inc_return(volatile ldus_atomic *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#else
	return ++*ptr;
#endif
}

static inline uint32_t ldus_atomic_add_return(volatile ldus_atomic *ptr, uint32_t value) {
#if defined(LDUS_USE_ATOMIC)
    return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
#else
    return *ptr += value;
#endif
}

static inline uint32_t ldus_atomic_dec_return(volatile ldus_atomic *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#else
	return --*ptr;
#endif
}

static inline void ldus_atomic_set(volatile ldus_atomic *ptr, uint32_t value) {
#if defined(LDUS_USE_ATOMIC)
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
#else
	*ptr = value;
#endif
}

static inline uint32_t ldus_atomic_cmpxchg(volatile ldus_atomic *ptr, uint32_t old_, uint32_t new_) {
#if defined(LDUS_USE_ATOMIC)
	__atomic_compare_exchange_n(ptr, &old_, new_, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return old_;
#else
	if (*ptr == old_) { *ptr = new_; return old_; }
	else return *ptr;
#endif
}

static inline uint64_t ldus_atomic64_read(volatile ldus_atomic64 *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#else
	return *ptr;
#endif
}

static inline uint64_t ldus_atomic64_inc_return(volatile ldus_atomic64 *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#else
	return ++*ptr;
#endif
}

static inline uint64_t ldus_atomic64_add_return(volatile ldus_atomic64 *ptr, uint64_t value) {
#if defined(LDUS_USE_ATOMIC)
    return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
#else
    return *ptr += value;
#endif
}

static inline uint64_t ldus_atomic64_dec_return(volatile ldus_atomic64 *ptr) {
#if defined(LDUS_USE_ATOMIC)
	return __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#else
	return --*ptr;
#endif
}

static inline void ldus_atomic64_set(volatile ldus_atomic64 *ptr, uint64_t value) {
#if defined(LDUS_USE_ATOMIC)
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
#else
	*ptr = value;
#endif
}

static inline uint64_t ldus_atomic64_cmpxchg(volatile ldus_atomic64 *ptr, uint64_t old_, uint64_t new_) {
#if defined(LDUS_USE_ATOMIC)
	__atomic_compare_exchange_n(ptr, &old_, new_, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return old_;
#else
	if (*ptr == old_) { *ptr = new_; return old_; }
	else return *ptr;
#endif
}

#endif
