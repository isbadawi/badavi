#include "mode.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "window.h"
#include "util.h"

struct operator_pending_mode {
  struct editing_mode mode;
  op_func* op;
};

static void yank_op(struct editor *editor, struct region *region) {
  struct buf *reg = editor_get_register(editor, editor->register_);
  struct gapbuf *gb = editor->window->buffer->text;

  size_t n = region->end - region->start;
  buf_grow(reg, n + 1);
  gb_getstring(gb, region->start, n, reg->buf);
  reg->len = n;
  editor->register_ = '"';
}

static void delete_op(struct editor *editor, struct region *region) {
  yank_op(editor, region);
  buffer_start_action_group(editor->window->buffer);
  buffer_do_delete(editor->window->buffer, region->end - region->start, region->start);
  window_set_cursor(editor->window, region->start);
}

static void change_op(struct editor *editor, struct region *region) {
  delete_op(editor, region);
  editor_push_mode(editor, insert_mode());
}

static struct { char name; op_func *op; } op_table[] = {
  {'d', delete_op},
  {'c', change_op},
  {'y', yank_op},
  {-1, NULL}
};

op_func *op_find(char name) {
  for (int i = 0; op_table[i].op; ++i) {
    if (op_table[i].name == name) {
      return op_table[i].op;
    }
  }
  return NULL;
}

static void key_pressed(struct editor *editor, struct tb_event *ev) {
  if (ev->ch != '0' && isdigit((int) ev->ch)) {
    editor->count = 0;
    while (isdigit((int) ev->ch)) {
      editor->count *= 10;
      editor->count += ev->ch - '0';
      editor_waitkey(editor, ev);
    }
  }

  struct motion *motion = motion_get(editor, ev);
  if (!motion) {
    editor_pop_mode(editor);
    return;
  }

  struct gapbuf *gb = editor->window->buffer->text;

  struct region *region = region_create(
      window_cursor(editor->window), motion_apply(motion, editor));

  ssize_t last = gb_lastindexof(gb, '\n', region->start - 1);
  size_t next = gb_indexof(gb, '\n', region->end);
  if (motion->linewise) {
    region->start = (size_t)(last + 1);
    region->end = min(gb_size(gb), next + 1);
  } else if (region->start == region->end) {
    editor_pop_mode(editor);
    return;
  } else if (!motion->exclusive) {
    region->end = min(region->end + 1, next);
  }

  struct operator_pending_mode* mode = (struct operator_pending_mode*) editor->mode;
  editor_pop_mode(editor);
  mode->op(editor, region);
  free(region);
}

static struct operator_pending_mode impl = {
  {
    .entered = NULL,
    .exited = NULL,
    .key_pressed = key_pressed,
    NULL
  },
  NULL
};

struct editing_mode *operator_pending_mode(char op_name) {
  op_func *op = op_find(op_name);
  if (!op) {
    return NULL;
  }
  impl.op = op;
  return (struct editing_mode*) &impl;
}
