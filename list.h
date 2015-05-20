#ifndef _list_h_included
#define _list_h_included

#include <stdbool.h>

typedef struct list_node_t {
  void *data;
  struct list_node_t *prev;
  struct list_node_t *next;
} list_node_t;

// A doubly linked list.
// Head and tail are sentinel nodes with data == NULL.
typedef struct {
  list_node_t *head;
  list_node_t *tail;

  // Used by LIST_FOREACH for iterating. (No concurrent iteration.)
  list_node_t *iter;
} list_t;

#define LIST_FOREACH(list, i) \
  for (list->iter = list->head->next, \
       i = list->iter->data; \
       list->iter != list->tail; \
       list->iter = list->iter->next, \
       i = list->iter->data)

#define LIST_FOREACH_REVERSE(list, i) \
  for (list->iter = list->tail->prev, \
      i = list->iter->data; \
      list->iter != list->head; \
      list->iter = list->iter->prev, \
      i = list->iter->data)

list_t *list_create(void);
void list_prepend(list_t* list, void *data);
void list_append(list_t* list, void *data);
void *list_pop(list_t *list);
void *list_peek(list_t *list);
bool list_empty(list_t *list);
void list_clear(list_t *list);

#endif
