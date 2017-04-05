#include "list.h"

#include <stdlib.h>

#include "util.h"

static struct list_node *list_node_create(void *data,
                                     struct list_node *prev,
                                     struct list_node *next) {
  struct list_node *node = xmalloc(sizeof(struct list_node));
  node->data = data;
  node->prev = prev;
  node->next = next;
  return node;
}

struct list *list_create(void) {
  struct list *list = xmalloc(sizeof(*list));
  list->tail = list_node_create(NULL, NULL, NULL);
  list->head = list_node_create(NULL, NULL, list->tail);
  list->tail->prev = list->head;
  return list;
}

struct list *list_steal(struct list_node *start, struct list_node *end) {
  start->prev->next = end->next;
  end->next->prev = start->prev;

  struct list *list = list_create();
  list->head->next = start;
  start->prev = list->head;
  list->tail->prev = end;
  end->next = list->tail;
  return list;
}

void list_free(struct list *list, list_free_func *free_func) {
  list_clear(list, free_func);
  free(list->head);
  free(list->tail);
  free(list);
}

static void list_insert_after_node(struct list_node *node, void *data) {
  struct list_node *newnode = list_node_create(data, node, node->next);
  node->next->prev = newnode;
  node->next = newnode;
}

static void list_insert_before_node(struct list_node *node, void *data) {
  struct list_node *newnode = list_node_create(data, node->prev, node);
  node->prev->next = newnode;
  node->prev = newnode;
}

void list_prepend(struct list *list, void *data) {
  list_insert_after_node(list->head, data);
}

void list_append(struct list *list, void *data) {
  list_insert_before_node(list->tail, data);
}

static void *list_remove_node(struct list_node *node) {
  void *data = node->data;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  free(node);
  return data;
}

struct list_node *list_get_node(struct list *list, void *data) {
  void *p;
  LIST_FOREACH(list, p) {
    if (p == data) {
      return list->iter;
    }
  }
  return NULL;
}

void *list_pop(struct list *list) {
  return list_empty(list) ? NULL : list_remove_node(list->head->next);
}

void list_remove(struct list *list, void *data) {
  list_remove_node(list_get_node(list, data));
}

void *list_first(struct list *list) {
  return list_empty(list) ? NULL : list->head->next->data;
}

void *list_last(struct list *list) {
  return list_empty(list) ? NULL : list->tail->prev->data;
}

bool list_empty(struct list *list) {
  return list->head->next == list->tail;
}

void list_clear(struct list *list, list_free_func *free_func) {
  void *data;
  struct list_node *prev = NULL;
  LIST_FOREACH(list, data) {
    if (prev) {
      free(prev);
    }
    if (free_func) {
      free_func(data);
    }
    prev = list->iter;
  }
  if (prev) {
    free(prev);
  }
  list->head->next = list->tail;
  list->tail->prev = list->head;
}

void *list_prev(struct list *list, void *data) {
  return list_get_node(list, data)->prev->data;
}

void *list_next(struct list *list, void *data) {
  return list_get_node(list, data)->next->data;
}

void list_insert_after(struct list *list, void *el, void *data) {
  list_insert_after_node(list_get_node(list, el), data);
}

void list_insert_before(struct list *list, void *el, void *data) {
  list_insert_before_node(list_get_node(list, el), data);
}

size_t list_size(struct list *list) {
  size_t size = 0;
  void *p;
  LIST_FOREACH(list, p) {
    size++;
  }
  return size;
}
