#pragma once

#include <stdbool.h>

#include <sys/queue.h>

struct history_entry {
  struct buf *buf;

  TAILQ_ENTRY(history_entry) pointers;
};

struct history {
  TAILQ_HEAD(history_list, history_entry) entries;
  int len;

  // How many history entries we keep (older ones get discarded).
  // This can change between calls if the user runs 'set history=val'.
  // The new limit only takes effect the next time history_add_item is called.
  // TODO(ibadawi): maybe have callbacks that run when option values change?
  int *limit;
};

void history_init(struct history *history, int *limit);
void history_deinit(struct history *history);
void history_add_item(struct history *history, char *item);

typedef bool (history_predicate) (struct buf*, char*);

struct history_entry *history_first(
    struct history *history, history_predicate p, char *arg);
struct history_entry *history_prev(
    struct history_entry *entry, history_predicate p, char *arg);
struct history_entry *history_next(
    struct history_entry *entry, history_predicate p, char *arg);
