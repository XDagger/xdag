/* FIFO-мьютексы и другие изменения стандартных фукнций из pthread.h; T13.093-T13.760; $DVS:time$ */

#ifndef DTHREAD_H_INCLUDED
#define DTHREAD_H_INCLUDED

#include <pthread.h>

#if !defined(__LDuS__) && !defined(_WIN32) && !defined(_WIN64)

#include <sched.h>
#include "../ldus/source/include/ldus/atomic.h"

typedef struct dthread_mutex {
	ldus_atomic head, tail;
} dthread_mutex_t;

static inline int dthread_mutex_init(dthread_mutex_t *mutex, const pthread_mutexattr_t *attr __attribute__((unused))) {
	ldus_atomic_set(&mutex->head, 0);
	ldus_atomic_set(&mutex->tail, 0);
	return 0;
}

static inline int dthread_mutex_destroy(dthread_mutex_t *mutex __attribute__((unused))) {
	return 0;
}

static inline int dthread_mutex_lock(dthread_mutex_t *mutex) {
	uint32_t ticket = ldus_atomic_inc_return(&mutex->tail) - 1;
	while (ticket != ldus_atomic_read(&mutex->head)) {
		sched_yield();
	}
	return 0;
}

static inline int dthread_mutex_unlock(dthread_mutex_t *mutex) {
	ldus_atomic_inc_return(&mutex->head);
	return 0;
}

#else

#define dthread_mutex_t			pthread_mutex_t
#define dthread_mutex_init		pthread_mutex_init
#define dthread_mutex_lock		pthread_mutex_lock
#define dthread_mutex_unlock	pthread_mutex_unlock
#define dthread_mutex_destroy	pthread_mutex_destroy

#endif

#endif
