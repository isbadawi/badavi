#include "mode.h"

#include <ctype.h>
#include <stddef.h>

#include <termbox.h>

#include "attrs.h"
#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "window.h"
#include "util.h"

static void yank_op(struct editor *editor, struct region *region) {
  struct editor_register *reg = editor_get_register(editor, editor->register_);
  struct gapbuf *gb = editor->window->buffer->text;

  size_t n = region->end - region->start;
  struct buf *buf = gb_getstring(gb, region->start, n);
  reg->write(reg, buf->buf);
  buf_free(buf);
  editor->register_ = '"';
}

static void delete_op(struct editor *editor, struct region *region) {
  if (!editor_try_modify(editor)) {
    return;
  }

  yank_op(editor, region);
  if (region->end == gb_size(editor->window->buffer->text)) {
    region->end--;
  }

  buffer_start_action_group(editor->window->buffer);
  buffer_do_delete(editor->window->buffer, region->end - region->start, region->start);
  window_set_cursor(editor->window, region->start);
}

static void change_op(struct editor *editor, struct region *region) {
  if (!editor_try_modify(editor)) {
    return;
  }

  delete_op(editor, region);
  editor_push_insert_mode(editor, 0);
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

void operator_pending_mode_entered(struct editor *editor) {
  struct operator_pending_mode *mode = editor_get_operator_pending_mode(editor);
  op_func *op = op_find((char) mode->mode.arg);
  if (!op) {
    editor_pop_mode(editor);
    return;
  }
  mode->op = op;
}

void operator_pending_mode_exited(struct editor *editor ATTR_UNUSED) {
  return;
}

void operator_pending_mode_key_pressed(
    struct editor *editor, struct tb_event *ev) {
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

  struct region region;
  region_set(&region,
      window_cursor(editor->window),
      motion_apply(motion, editor));

  ssize_t last = gb_lastindexof(gb, '\n', region.start - 1);
  size_t next = gb_indexof(gb, '\n', region.end);
  if (motion->linewise) {
    region.start = (size_t)(last + 1);
    region.end = min(gb_size(gb), next + 1);
  } else if (region.start == region.end) {
    editor_pop_mode(editor);
    return;
  } else if (!motion->exclusive) {
    region.end = min(region.end + 1, next);
  }

  struct operator_pending_mode* mode = editor_get_operator_pending_mode(editor);
  editor_pop_mode(editor);
  mode->op(editor, &region);
}
