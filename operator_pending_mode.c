#include "mode.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "window.h"
#include "undo.h"
#include "util.h"

struct region_t {
  size_t start;
  size_t end;
};

static struct region_t region_create(size_t start, size_t end) {
  struct region_t region = {min(start, end), max(start, end)};
  return region;
}

typedef void (op_t) (struct editor_t *editor, struct region_t);

struct operator_pending_mode_t {
  struct editing_mode_t mode;
  op_t* op;
};

static void yank_op(struct editor_t *editor, struct region_t region) {
  struct buf_t *reg = editor_get_register(editor, editor->register_);
  struct gapbuf_t *gb = editor->window->buffer->text;

  size_t n = region.end - region.start;
  buf_grow(reg, n + 1);
  gb_getstring(gb, region.start, n, reg->buf);
  reg->len = n;
  editor_pop_mode(editor);
}

static void delete_op(struct editor_t *editor, struct region_t region) {
  yank_op(editor, region);
  struct gapbuf_t *gb = editor->window->buffer->text;

  struct buf_t *buf = buf_create(region.end - region.start + 1);
  gb_getstring(gb, region.start, region.end - region.start, buf->buf);
  buf->len = region.end - region.start;
  buf->buf[buf->len] = '\0';
  struct edit_action_t action = {
    .type = EDIT_ACTION_DELETE,
    .pos = region.start,
    .buf = buf
  };
  editor_start_action_group(editor);
  editor_add_action(editor, action);

  gb_del(gb, region.end - region.start, region.end);
  editor->window->buffer->dirty = true;
}

static void change_op(struct editor_t *editor, struct region_t region) {
  delete_op(editor, region);
  editor_push_mode(editor, insert_mode());
}

static struct { char name; op_t *op; } op_table[] = {
  {'d', delete_op},
  {'c', change_op},
  {'y', yank_op},
  {-1, NULL}
};

static op_t *op_find(char name) {
  for (int i = 0; op_table[i].op; ++i) {
    if (op_table[i].name == name) {
      return op_table[i].op;
    }
  }
  return NULL;
}

static void entered(struct editor_t *editor) {
  if (!editor->motion) {
    return;
  }

  struct gapbuf_t *gb = editor->window->buffer->text;
  struct motion_t *motion = editor->motion;

  struct region_t region = region_create(
      editor->window->cursor, motion_apply(editor));

  ssize_t last = gb_lastindexof(gb, '\n', region.start - 1);
  size_t next = gb_indexof(gb, '\n', region.end);
  if (motion->linewise) {
    region.start = max(0, (size_t) (last + 1));
    region.end = min(gb_size(gb), next + 1);
  } else if (region.start == region.end) {
    editor_pop_mode(editor);
    return;
  } else if (!motion->exclusive) {
    region.end = min(region.end + 1, next);
  }

  struct operator_pending_mode_t* mode = (struct operator_pending_mode_t*) editor->mode;
  mode->op(editor, region);
  editor->register_ = '"';

  editor->window->cursor = region.start;
  if (editor->window->cursor > gb_size(gb) - 1) {
    editor->window->cursor = (size_t) (gb_lastindexof(gb, '\n', (size_t) (last - 1)) + 1);
  }
}

static void key_pressed(struct editor_t *editor, struct tb_event *ev) {
  bool digit = ev->ch != '0' && isdigit((int) ev->ch);
  editor_push_mode(editor, digit ? digit_mode() : motion_mode());
  editor_handle_key_press(editor, ev);
}

static struct operator_pending_mode_t impl = {
  {entered, key_pressed, NULL},
  NULL
};

struct editing_mode_t *operator_pending_mode(char op_name) {
  op_t *op = op_find(op_name);
  if (!op) {
    return NULL;
  }
  impl.op = op;
  return (struct editing_mode_t*) &impl;
}
