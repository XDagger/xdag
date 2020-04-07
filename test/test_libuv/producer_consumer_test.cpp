#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <gtest/gtest.h>
#include "queue.h"
#include "uv.h"

#define MAX_LOOPS 1000000

struct buffer_s {
    xd_queue_t queue;
    int data;
};
typedef struct buffer_s buffer_t;

static xd_queue_t queue;
static uv_mutex_t mutex;
static uv_cond_t empty;
static uv_cond_t full;

void produce(int value) {
    buffer_t *buf;
    buf = (buffer_t*)malloc(sizeof(buffer_t));
    xd_queue_init(&buf->queue);
    buf->data = value;
    xd_queue_insert_tail(&queue, &buf->queue);
}

int consume(void) {
    xd_queue_t *q;
    buffer_t *buf;
    int data;
    assert(!xd_queue_empty(&queue));
    q = xd_queue_last(&queue);
    xd_queue_remove(q);

    buf = xd_queue_data(q, buffer_t, queue);
    data = buf->data;
    free(buf);
    return data;
}

static void producer(void* arg) {
    int i;

    for(i = 0; i < MAX_LOOPS * 10; i++) {
        uv_mutex_lock(&mutex);
        while(!xd_queue_empty(&queue)) {
            uv_cond_wait(&empty, &mutex);
        }
        produce(i);

        fprintf(stdout, "put %d\n", i);
        uv_cond_signal(&full);
        uv_mutex_unlock(&mutex);
    }

}

static void consumer(void* arg) {
    int i;
    int value;

    (void) arg;

    while(1) {
        uv_mutex_lock(&mutex);
        while(xd_queue_empty(&queue)) {
            uv_cond_wait(&full, &mutex);
        }
        value = consume();
        fprintf(stdout, "get %d\n", value);
        uv_cond_signal(&empty);
        uv_mutex_unlock(&mutex);
    }
}

TEST(libUV, producerConsumerTest) {
    uv_thread_t producer_thread;
    uv_thread_t consumer_pthread;

    xd_queue_init(&queue);
    ASSERT_EQ(0,uv_mutex_init(&mutex)) << "uv mutex init success";
	ASSERT_EQ(0,uv_cond_init(&empty)) << "uv cond init empty success";
	ASSERT_EQ(0,uv_cond_init(&full)) << "uv cond init full success";
	
	ASSERT_EQ(0,uv_thread_create(&producer_thread, producer, NULL)) << "uv producer thread created";
	ASSERT_EQ(0,uv_thread_create(&consumer_pthread, consumer, NULL)) << "uv consumer thread created";
	
	ASSERT_EQ(0,uv_thread_join(&producer_thread)) << "uv producer thread joined";;
	ASSERT_EQ(0,uv_thread_join(&consumer_pthread)) << "uv consumer thread created";;

    uv_cond_destroy(&empty);
    uv_cond_destroy(&full);
    uv_mutex_destroy(&mutex);
}