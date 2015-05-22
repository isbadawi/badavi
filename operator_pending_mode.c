#include "mode.h"

#include <ctype.h>
#include <stdlib.h>

#include <termbox.h>

#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "util.h"

typedef struct {
  int start;
  int end;
} region_t;

static region_t region_create(int start, int end) {
  region_t region = {min(start, end), max(start, end)};
  return region;
}

typedef void (op_t) (editor_t *editor, region_t);

typedef struct {
  editing_mode_t mode;
  op_t* op;
} operator_pending_mode_t;


static void yank_op(editor_t *editor, region_t region) {
  buf_t *reg = editor_get_register(editor, editor->register_);
  gapbuf_t *gb = editor->window->buffer->text;

  int n = region.end - region.start;
  buf_grow(reg, n + 1);
  gb_getstring(gb, region.start, n, reg->buf);
  reg->len = n;
  editor_pop_mode(editor);
}

static void delete_op(editor_t *editor, region_t region) {
  yank_op(editor, region);
  gapbuf_t *gb = editor->window->buffer->text;

  buf_t *buf = buf_create(region.end - region.start + 1);
  gb_getstring(gb, region.start, region.end - region.start, buf->buf);
  buf->len = region.end - region.start;
  buf->buf[buf->len] = '\0';
  edit_action_t action = {
    .type = EDIT_ACTION_DELETE,
    .pos = region.start,
    .buf = buf
  };
  editor_start_action_group(editor);
  editor_add_action(editor, action);

  gb_del(gb, region.end - region.start, region.end);
  editor->window->buffer->dirty = true;
}

static void change_op(editor_t *editor, region_t region) {
  delete_op(editor, region);
  editor_push_mode(editor, insert_mode());
}

typedef struct {
  char name;
  op_t* op;
} op_table_entry_t;

static op_table_entry_t op_table[] = {
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

static void entered(editor_t *editor) {
  if (!editor->motion) {
    return;
  }

  gapbuf_t *gb = editor->window->buffer->text;
  motion_t *motion = editor->motion;

  region_t region = region_create(
      editor->window->cursor, motion_apply(editor));


  int last = gb_lastindexof(gb, '\n', region.start - 1);
  int next = gb_indexof(gb, '\n', region.end);
  if (motion->linewise) {
    region.start = max(0, last + 1);
    region.end = min(gb_size(gb), next + 1);
  } else if (region.start == region.end) {
    editor_pop_mode(editor);
    return;
  } else if (!motion->exclusive) {
    region.end = min(region.end + 1, next);
  }

  operator_pending_mode_t* mode = (operator_pending_mode_t*) editor->mode;
  mode->op(editor, region);
  editor->register_ = '"';

  editor->window->cursor = region.start;
  if (editor->window->cursor > gb_size(gb) - 1) {
    editor->window->cursor = gb_lastindexof(gb, '\n', last - 1) + 1;
  }
}

static void key_pressed(editor_t *editor, struct tb_event *ev) {
  if (ev->ch != '0' && isdigit(ev->ch)) {
    editor_push_mode(editor, digit_mode());
    editor_handle_key_press(editor, ev);
    return;
  } else {
    editor_push_mode(editor, motion_mode());
    editor_handle_key_press(editor, ev);
  }
}

static operator_pending_mode_t impl = {
  {entered, key_pressed, NULL},
  NULL
};

editing_mode_t *operator_pending_mode(char op_name) {
  op_t *op = op_find(op_name);
  if (!op) {
    return NULL;
  }
  impl.op = op;
  return (editing_mode_t*) &impl;
}
