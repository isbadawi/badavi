#pragma once

#include <stdbool.h>
#include <stddef.h>

struct motion_context {
  size_t pos;
  struct window *window;
  struct editor *editor;
};

struct motion {
  char name;
  size_t (*op)(struct motion_context);
  // Whether the motion is linewise (vs. characterwise).
  // (See :help linewise in vim for details).
  bool linewise;
  // Whether the motion is exclusive (vs. inclusive).
  // (See :help exclusive in vim for details).
  // This is only relevant for characterwise motions.
  bool exclusive;
};

struct tb_event;
struct motion *motion_get(struct editor *editor, struct tb_event *ev);

size_t motion_apply(struct motion *motion, struct editor *editor);

// TODO(isbadawi): This is an awkward place to put this.
// But a lot of knowledge about words lives in motion.c right now.
struct buf *motion_word_under_cursor(struct window *window);
