#include "mode.h"

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "list.h"
#include "window.h"

static void insert_mode_entered(struct editor_t *editor) {
  editor_status_msg(editor, "-- INSERT --");
  buffer_start_action_group(editor->window->buffer);
}

static void insert_mode_exited(struct editor_t *editor) {
  // If we exit insert mode without making changes, let's not add a
  // useless undo action.
  // TODO(isbadawi): Maybe this belongs in an "editor_end_action_group"
  struct list_t *undo_stack = editor->window->buffer->undo_stack;
  if (list_empty(list_peek(undo_stack))) {
    list_pop(undo_stack);
  }

  buf_clear(editor->status);
}

static void insert_mode_key_pressed(struct editor_t* editor, struct tb_event* ev) {
  struct buffer_t *buffer = editor->window->buffer;
  size_t cursor = window_cursor(editor->window);
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    editor_pop_mode(editor);
    return;
  case TB_KEY_BACKSPACE2:
    if (cursor > 0) {
      buffer_do_delete(buffer, 1, cursor - 1);
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  default: ch = (char) ev->ch; break;
  }
  buffer_do_insert(buffer, buf_from_char(ch), cursor);
}

static struct editing_mode_t impl = {
  .entered = insert_mode_entered,
  .exited = insert_mode_exited,
  .key_pressed = insert_mode_key_pressed,
  .parent = NULL
};

struct editing_mode_t *insert_mode(void) {
  return &impl;
}
