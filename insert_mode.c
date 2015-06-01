#include "mode.h"

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "editor.h"
#include "list.h"
#include "window.h"
#include "undo.h"

static void insert_mode_entered(struct editor_t *editor) {
  editor_status_msg(editor, "-- INSERT --");
  editor_start_action_group(editor);
}

static void insert_mode_key_pressed(struct editor_t* editor, struct tb_event* ev) {
  struct buffer_t *buffer = editor->window->buffer;
  struct gapbuf_t *gb = buffer->text;
  size_t *cursor = &editor->window->cursor;
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
      buffer->dirty = true;
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  default: ch = (char) ev->ch; break;
  }
  edit_action_t action = {
    .type = EDIT_ACTION_INSERT,
    .pos = *cursor,
    .buf = buf_from_char(ch)
  };
  editor_add_action(editor, action);
  gb_putchar(gb, ch, (*cursor)++);
  buffer->dirty = true;
}

static struct editing_mode_t impl = {
  insert_mode_entered,
  insert_mode_key_pressed,
  NULL
};

struct editing_mode_t *insert_mode(void) {
  return &impl;
}
