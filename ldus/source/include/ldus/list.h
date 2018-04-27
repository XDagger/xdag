/* работа с циклическим списком, T10.544-T11.692; $DVS:time$ */

#ifndef	LDUS_LIST_H_INCLUDED
#define LDUS_LIST_H_INCLUDED

#include <stddef.h>

// get rid of access memory 0x0000000000000000 runtime error which will cause dnet restart.
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)offsetof(type, member)))
//#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)&((type *)0)->member))

/* циклический список */
struct list {
	struct list *prev;
	struct list *next;
};

/* создать пустой список с данной головой */
static inline void list_init(struct list *head) {
	head->prev = head->next = head;
}

/* добавить узел в список */
static inline void list_insert(struct list *head, struct list *node) {
	node->prev = head, node->next = head->next;
	node->next->prev = head->next = node;
}

/* добавить узел в список с конца */
static inline void list_insert_before(struct list *head, struct list *node) {
	node->next = head, node->prev = head->prev;
	node->prev->next = head->prev = node;
}

/* удалить узел из списка */
static inline void list_remove(struct list *node) {
	struct list *prev = node->prev, *next = node->next;
	prev->next = next, next->prev = prev;
	node->prev = node->next = node;
}

#endif
