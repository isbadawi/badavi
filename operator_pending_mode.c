#include "mode.h"

#include <stdlib.h>

#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "util.h"

typedef struct {
  int start;
  int end;
} region_t;

region_t region_create(int start, int end) {
  region_t region = {min(start, end), max(start, end)};
  return region;
}

typedef void (op_t) (editor_t *editor, region_t);

typedef struct {
  editing_mode_t mode;
  op_t* op;
} operator_pending_mode_t;

static void delete_op(editor_t *editor, region_t region) {
  gapbuf_t *gb = editor->window->buffer->text;
  gb_del(gb, region.end - region.start, region.end);
  editor->window->buffer->dirty = 1;
  editor->mode = normal_mode();
}

static void change_op(editor_t *editor, region_t region) {
  delete_op(editor, region);
  // TODO(isbadawi): This is duplicated between normal mode and here.
  editor_status_msg(editor, "-- INSERT --");
  editor->mode = insert_mode();
}

typedef struct {
  char name;
  op_t* op;
} op_table_entry_t;

static op_table_entry_t op_table[] = {
  {'d', delete_op},
  {'c', change_op},
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

static void key_pressed(editor_t *editor, struct tb_event *ev) {
  static motion_t motion = {NULL, 0, 0};
  operator_pending_mode_t* mode = (operator_pending_mode_t*) editor->mode;

  int rc = motion_key(&motion, ev);
  if (rc == 1) {
    region_t region = region_create(
        editor->window->cursor,
        motion_apply(&motion, editor->window));
    editor->window->cursor = region.start;
    mode->op(editor, region);
  } else if (rc < 0) {
    // If the character doesn't make sense, go back to normal mode
    editor->mode = normal_mode();
  }
}

static operator_pending_mode_t impl = {{key_pressed}, NULL};

editing_mode_t *operator_pending_mode(char op_name) {
  op_t *op = op_find(op_name);
  if (!op) {
    return NULL;
  }
  impl.op = op;
  return (editing_mode_t*) &impl;
}
