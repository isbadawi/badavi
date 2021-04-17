#include "history.h"

#include <stdlib.h>

#include "buf.h"
#include "util.h"

void history_init(struct history *history, int *limit) {
  TAILQ_INIT(&history->entries);
  history->len = 0;
  history->limit = limit;
}

void history_deinit(struct history *history) {
  struct history_entry *entry, *te;
  TAILQ_FOREACH_SAFE(entry, &history->entries, pointers, te) {
    buf_free(entry->buf);
    free(entry);
  }
}

struct history_entry *history_first(
    struct history *history, history_predicate p, char* arg) {
  struct history_entry *entry = TAILQ_FIRST(&history->entries);
  if (!entry || p(entry->buf, arg)) {
    return entry;
  }
  return history_next(entry, p, arg);
}

struct history_entry *history_next(
    struct history_entry *entry, history_predicate p, char* arg) {
  struct history_entry *next = entry;
  while ((next = TAILQ_NEXT(next, pointers))) {
    if (p(next->buf, arg)) {
      return next;
    }
  }
  return NULL;
}

struct history_entry *history_prev(
    struct history_entry *entry, history_predicate p, char* arg) {
  struct history_entry *prev = entry;
  while ((prev = TAILQ_PREV(prev, history_list, pointers))) {
    if (p(prev->buf, arg)) {
      return prev;
    }
  }
  return NULL;
}

static void history_truncate_to_limit(struct history *history) {
  while (history->limit && history->len > *history->limit) {
    struct history_entry *last = TAILQ_LAST(&history->entries, history_list);
    TAILQ_REMOVE(&history->entries, last, pointers);
    buf_free(last->buf);
    free(last);
    history->len--;
  }
}

void history_add_item(struct history *history, char *item) {
  struct history_entry *entry = history_first(history, buf_equals, item);
  if (entry) {
    TAILQ_REMOVE(&history->entries, entry, pointers);
    history->len--;
  }
  history_truncate_to_limit(history);

  if (!entry) {
    entry = xmalloc(sizeof(*entry));
    entry->buf = buf_from_cstr(item);
  }
  TAILQ_INSERT_HEAD(&history->entries, entry, pointers);
  history->len++;
}
