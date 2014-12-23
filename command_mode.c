#include "mode.h"

#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"

// TODO(isbadawi): Command mode needs a cursor, but modes don't affect drawing.
static void command_mode_key_pressed(editor_t *editor, struct tb_event *ev) {
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC:
    buf_printf(editor->status, "");
    editor->mode = normal_mode();
    return;
  case TB_KEY_BACKSPACE2:
    buf_delete(editor->status, editor->status->len - 1, 1);
    if (editor->status->len == 0) {
      editor->mode = normal_mode();
      return;
    }
    return;
  case TB_KEY_ENTER: {
    char *command = strdup(editor->status->buf + 1);
    editor_execute_command(editor, command);
    free(command);
    editor->mode = normal_mode();
    return;
  }
  case TB_KEY_SPACE:
    ch = ' ';
    break;
  default:
    ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_insert(editor->status, s, editor->status->len);
}

static editing_mode_t impl = {command_mode_key_pressed};

editing_mode_t *command_mode(void) {
  return &impl;
}
