#include "mode.h"

#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"

static void command_mode_entered(editor_t *editor) {
  editor_status_msg(editor, ":");
}

// TODO(isbadawi): Command mode needs a cursor, but modes don't affect drawing.
static void command_mode_key_pressed(editor_t *editor, struct tb_event *ev) {
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    buf_printf(editor->status, "");
    editor_pop_mode(editor);
    return;
  case TB_KEY_BACKSPACE2:
    buf_delete(editor->status, editor->status->len - 1, 1);
    if (editor->status->len == 0) {
      editor_pop_mode(editor);
      return;
    }
    return;
  case TB_KEY_ENTER: {
    if (editor->status->len > 1) {
      char *command = strndup(editor->status->buf + 1, editor->status->len - 1);
      editor_execute_command(editor, command);
      free(command);
    }
    editor_pop_mode(editor);
    return;
  }
  case TB_KEY_SPACE:
    ch = ' ';
    break;
  default:
    ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_append(editor->status, s);
}

static editing_mode_t impl = {
  command_mode_entered,
  command_mode_key_pressed
};

editing_mode_t *command_mode(void) {
  return &impl;
}
