#include "queue.h"

xd_queue_t* xd_queue_middle(xd_queue_t *queue)
{
    xd_queue_t  *middle, *next;

    middle = xd_queue_head(queue);

    if (middle == xd_queue_last(queue)) {
        return middle;
    }

    next = xd_queue_head(queue);

    for ( ;; ) {
        middle = xd_queue_next(middle);

        next = xd_queue_next(next);

        if (next == xd_queue_last(queue)) {
            return middle;
        }

        next = xd_queue_next(next);

        if (next == xd_queue_last(queue)) {
            return middle;
        }
    }
}


/* the stable insertion sort */

void xd_queue_sort(xd_queue_t *queue, int (*cmp)(const xd_queue_t *, const xd_queue_t *))
{
    xd_queue_t  *q, *prev, *next;

    q = xd_queue_head(queue);

    if (q == xd_queue_last(queue)) {
        return;
    }

    for (q = xd_queue_next(q); q != xd_queue_sentinel(queue); q = next) {

        prev = xd_queue_prev(q);
        next = xd_queue_next(q);

        xd_queue_remove(q);

        do {
            if (cmp(prev, q) <= 0) {
                break;
            }

            prev = xd_queue_prev(prev);

        } while (prev != xd_queue_sentinel(queue));

        xd_queue_insert_after(prev, q);
    }
}