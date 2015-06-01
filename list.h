#pragma once

#include <stdbool.h>
#include <stddef.h>

struct list_node_t {
  void *data;
  struct list_node_t *prev;
  struct list_node_t *next;
};

// A doubly linked list.
// Head and tail are sentinel nodes with data == NULL.
// n.b. The list doesn't own its data -- it just stores pointers.
// It's up to the callers to free memory if required.
struct list_t {
  struct list_node_t *head;
  struct list_node_t *tail;

  // Used by LIST_FOREACH for iterating. (No concurrent iteration.)
  struct list_node_t *iter;
};

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

struct list_t *list_create(void);
void list_free(struct list_t *list);
void list_prepend(struct list_t* list, void *data);
void list_append(struct list_t* list, void *data);
void *list_pop(struct list_t *list);
void list_remove(struct list_t *list, void *data);
void *list_peek(struct list_t *list);
bool list_empty(struct list_t *list);
void list_clear(struct list_t *list);
void *list_prev(struct list_t *list, void *data);
void *list_next(struct list_t *list, void *data);
void list_insert_after(struct list_t *list, void *el, void *data);
void list_insert_before(struct list_t *list, void *el, void *data);
size_t list_size(struct list_t *list);
