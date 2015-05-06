#include "mode.h"

#include <termbox.h>

#include "buffer.h"
#include "gap.h"
#include "editor.h"

static void insert_mode_entered(editor_t *editor) {
  editor_status_msg(editor, "-- INSERT --");
  editor_start_action_group(editor);
}

static void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  buffer_t *buffer = editor->window->buffer;
  gapbuf_t *gb = buffer->text;
  int *cursor = &editor->window->cursor;
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    // If we exit insert mode without making changes, let's not add a
    // useless undo action.
    // TODO(isbadawi): Maybe this belongs in an "editor_end_action_group"
    if (list_empty(list_peek(buffer->undo_stack))) {
      list_pop(buffer->undo_stack);
    }

    editor_status_msg(editor, "");
    editor_pop_mode(editor);
    return;
  case TB_KEY_BACKSPACE2:
    if (*cursor > 0) {
      edit_action_t action = {
        .type = EDIT_ACTION_DELETE,
        .pos = *cursor - 1,
        .buf = buf_from_char(gb_getchar(gb, *cursor - 1))
      };
      editor_add_action(editor, action);
      gb_del(gb, 1, (*cursor)--);
      buffer->dirty = 1;
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  default: ch = ev->ch; break;
  }
  edit_action_t action = {
    .type = EDIT_ACTION_INSERT,
    .pos = *cursor,
    .buf = buf_from_char(ch)
  };
  editor_add_action(editor, action);
  gb_putchar(gb, ch, (*cursor)++);
  buffer->dirty = 1;
}

static editing_mode_t impl = {
  insert_mode_entered,
  insert_mode_key_pressed
};

editing_mode_t *insert_mode(void) {
  return &impl;
}
