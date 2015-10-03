#pragma once

#include <stdbool.h>
#include <stddef.h>

struct motion_context_t {
  size_t pos;
  struct window_t *window;
  struct editor_t *editor;
};

struct motion_t {
  char name;
  size_t (*op)(struct motion_context_t);
  // Whether the motion is linewise (vs. characterwise).
  // (See :help linewise in vim for details).
  bool linewise;
  // Whether the motion is exclusive (vs. inclusive).
  // (See :help exclusive in vim for details).
  // This is only relevant for characterwise motions.
  bool exclusive;
};

struct tb_event;
struct motion_t *motion_get(struct editor_t *editor, struct tb_event *ev);

size_t motion_apply(struct motion_t *motion, struct editor_t *editor);

// TODO(isbadawi): This is an awkward place to put this.
// But a lot of knowledge about words lives in motion.c right now.
size_t motion_word_under_cursor(struct window_t *window, char *buf);
