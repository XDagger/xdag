//
// Created by reymondtu on 2020/4/2.
//

#ifndef XDAG_QUEUE_H
#define XDAG_QUEUE_H


typedef struct xd_queue_s  xd_queue_t;

struct xd_queue_s {
    xd_queue_t  *prev;
    xd_queue_t  *next;
};


#define xd_queue_init(q)                                                      \
    (q)->prev = q;                                                            \
    (q)->next = q


#define xd_queue_empty(h)                                                     \
    (h == (h)->prev)


#define xd_queue_insert_head(h, x)                                            \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x


#define xd_queue_insert_after   xd_queue_insert_head


#define xd_queue_insert_tail(h, x)                                            \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


#define xd_queue_head(h)                                                      \
    (h)->next


#define xd_queue_last(h)                                                      \
    (h)->prev


#define xd_queue_sentinel(h)                                                  \
    (h)


#define xd_queue_next(q)                                                      \
    (q)->next


#define xd_queue_prev(q)                                                      \
    (q)->prev


#define xd_queue_remove(x)                                                    \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next



#define xd_queue_split(h, q, n)                                               \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


#define xd_queue_add(h, n)                                                    \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


#define xd_queue_data(q, type, link)                                          \
    (type *) ((u_char *) q - offsetof(type, link))


xd_queue_t *xd_queue_middle(xd_queue_t *queue);
void xd_queue_sort(xd_queue_t *queue, int (*cmp)(const xd_queue_t *, const xd_queue_t *));


#endif //XDAG_QUEUE_H
