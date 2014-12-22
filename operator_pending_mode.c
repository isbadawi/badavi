#include "mode.h"

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "motion.h"
#include "util.h"

typedef struct {
  pos_t start;
  pos_t end;
} region_t;

// Create a region from the two endpoints, which can be in either order --
// they'll be swapped if necessary so that start occurs before end.
region_t region_create(buffer_t *buffer, pos_t a, pos_t b) {
  int start_line = buffer_index_of_line(buffer, a.line);
  int end_line = buffer_index_of_line(buffer, b.line);

  region_t result;
  if (start_line < end_line) {
    result.start = a;
    result.end = b;
  } else if (start_line > end_line) {
    result.start = b;
    result.end = a;
  } else if (a.offset < b.offset) {
    result.start = a;
    result.end = b;
  } else {
    result.start = b;
    result.end = a;
  }
  return result;
}

typedef void (op_t) (editor_t *editor, region_t);

typedef struct {
  editing_mode_t mode;
  op_t* op;
} operator_pending_mode_t;

static void delete_op(editor_t *editor, region_t region) {
  line_t *start_line = region.start.line;
  line_t *end_line = region.end.line;
  if (start_line == end_line) {
    int start = min(region.start.offset, region.end.offset);
    int end = max(region.start.offset, region.end.offset);
    buf_delete(start_line->buf, start, end - start);
  } else {
    // Delete from start offset to end of start line.
    buf_t *buf = start_line->buf;
    int offset = region.start.offset;
    buf_delete(buf, offset, buf->len - offset);
    // Remove any intermediate lines.
    line_t *line = start_line->next;
    while (line != end_line) {
      line_t *next = line->next;
      buffer_remove_line(editor->window->buffer, line);
      line = next;
    }
    // Delete from start of end line to end offset.
    buf = end_line->buf;
    offset = region.end.offset;
    buf_delete(buf, 0, offset);
    // Join the two remaining lines.
    buf_insert(
        start_line->buf,
        end_line->buf->buf,
        start_line->buf->len);
    buffer_remove_line(editor->window->buffer, end_line);
  }
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
        editor->window->buffer,
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
