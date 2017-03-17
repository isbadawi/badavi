#pragma once

#include <stdbool.h>
#include <stddef.h>

struct list_node {
  void *data;
  struct list_node *prev;
  struct list_node *next;
};

// A doubly linked list.
// Head and tail are sentinel nodes with data == NULL.
// n.b. The list doesn't own its data -- it just stores pointers.
// It's up to the callers to free memory if required.
struct list {
  struct list_node *head;
  struct list_node *tail;

  // Used by LIST_FOREACH for iterating. (No concurrent iteration.)
  struct list_node *iter;
};

#define LIST_FOREACH(list, i) \
  for (list->iter = list->head->next, \
       i = list->iter->data, \
       i = i; \
       list->iter != list->tail; \
       list->iter = list->iter->next, \
       i = list->iter->data)

#define LIST_FOREACH_REVERSE(list, i) \
  for (list->iter = list->tail->prev, \
      i = list->iter->data, \
      i = i; \
      list->iter != list->head; \
      list->iter = list->iter->prev, \
      i = list->iter->data)

typedef void (list_free_func)(void*);

struct list *list_create(void);
struct list *list_steal(struct list_node *start, struct list_node *end);
void list_free(struct list *list, list_free_func *free_func);
void list_prepend(struct list* list, void *data);
void list_append(struct list* list, void *data);
void *list_pop(struct list *list);
void list_remove(struct list *list, void *data);
void *list_peek(struct list *list);
bool list_empty(struct list *list);
void list_clear(struct list *list, list_free_func *free_func);
void *list_prev(struct list *list, void *data);
void *list_next(struct list *list, void *data);
void list_insert_after(struct list *list, void *el, void *data);
void list_insert_before(struct list *list, void *el, void *data);
size_t list_size(struct list *list);
struct list_node *list_get_node(struct list *list, void *data);
