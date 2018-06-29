/* работа с циклическим списком, T10.544-T11.692; $DVS:time$ */

#ifndef	LDUS_LIST_H_INCLUDED
#define LDUS_LIST_H_INCLUDED

#include <stddef.h>

// get rid of access memory 0x0000000000000000 runtime error which will cause dnet restart.
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)offsetof(type, member)))
//#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)&((type *)0)->member))

/* doubly linked list */
struct list {
	struct list *prev;
	struct list *next;
};

/* initialize an empty list */
static inline void list_init(struct list *head) {
	head->prev = head->next = head;
}

/* adds a node to the end of the list */
static inline void list_insert(struct list *head, struct list *node) {
	node->prev = head;
	node->next = head->next;
	node->next->prev = head->next = node;
}

/* adds the node to the beginning of the list */
static inline void list_insert_before(struct list *head, struct list *node) {
	node->next = head;
	node->prev = head->prev;
	node->prev->next = head->prev = node;
}

/* remove the node from the list */
static inline void list_remove(struct list *node) {
	struct list *prev = node->prev;
	struct list *next = node->next;
	prev->next = next;
	next->prev = prev;
	node->prev = node->next = node;
}

#endif
