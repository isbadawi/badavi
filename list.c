#include "list.h"

#include <stdlib.h>

static struct list_node_t *list_node_create(void *data,
                                     struct list_node_t *prev,
                                     struct list_node_t *next) {
  struct list_node_t *node = malloc(sizeof(struct list_node_t));
  if (!node) {
    return NULL;
  }
  node->data = data;
  node->prev = prev;
  node->next = next;
  return node;
}

struct list_t *list_create(void) {
  struct list_t *list = malloc(sizeof(struct list_t));
  if (!list) {
    return NULL;
  }
  list->tail = list_node_create(NULL, NULL, NULL);
  list->head = list_node_create(NULL, NULL, list->tail);
  list->tail->prev = list->head;
  return list;
}

void list_free(struct list_t *list) {
  list_clear(list);
  free(list->head);
  free(list->tail);
  free(list);
}

static void list_insert_after_node(struct list_node_t *node, void *data) {
  struct list_node_t *newnode = list_node_create(data, node, node->next);
  node->next->prev = newnode;
  node->next = newnode;
}

static void list_insert_before_node(struct list_node_t *node, void *data) {
  struct list_node_t *newnode = list_node_create(data, node->prev, node);
  node->prev->next = newnode;
  node->prev = newnode;
}

void list_prepend(struct list_t *list, void *data) {
  list_insert_after_node(list->head, data);
}

void list_append(struct list_t *list, void *data) {
  list_insert_before_node(list->tail, data);
}

static void *list_remove_node(struct list_node_t *node) {
  void *data = node->data;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  free(node);
  return data;
}

static struct list_node_t *list_get_node(struct list_t *list, void *data) {
  void *p;
  LIST_FOREACH(list, p) {
    if (p == data) {
      return list->iter;
    }
  }
  return NULL;
}

void *list_pop(struct list_t *list) {
  return list_empty(list) ? NULL : list_remove_node(list->head->next);
}

void list_remove(struct list_t *list, void *data) {
  list_remove_node(list_get_node(list, data));
}

void *list_peek(struct list_t *list) {
  return list_empty(list) ? NULL : list->head->next->data;
}

bool list_empty(struct list_t *list) {
  return list->head->next == list->tail;
}

void list_clear(struct list_t *list) {
  void *data;
  struct list_node_t *prev = NULL;
  LIST_FOREACH(list, data) {
    if (prev) {
      free(prev);
    }
    prev = list->iter;
  }
  if (prev) {
    free(prev);
  }
  list->head->next = list->tail;
  list->tail->prev = list->head;
}

void *list_prev(struct list_t *list, void *data) {
  return list_get_node(list, data)->prev->data;
}

void *list_next(struct list_t *list, void *data) {
  return list_get_node(list, data)->next->data;
}

void list_insert_after(struct list_t *list, void *el, void *data) {
  list_insert_after_node(list_get_node(list, el), data);
}

void list_insert_before(struct list_t *list, void *el, void *data) {
  list_insert_before_node(list_get_node(list, el), data);
}

size_t list_size(struct list_t *list) {
  size_t size = 0;
  void *p;
  LIST_FOREACH(list, p) {
    size++;
  }
  return size;
}
