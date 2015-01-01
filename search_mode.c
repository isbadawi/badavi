#include "mode.h"

#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "util.h"

// TODO(isbadawi): This is all very similar to command mode.
static void search_mode_entered(editor_t *editor) {
  editor_status_msg(editor, "/");
}

static void search_mode_key_pressed(editor_t *editor, struct tb_event *ev) {
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
      buf_t *reg = editor_get_register(editor, '/');
      buf_clear(reg);
      buf_insert_bytes(
          reg, editor->status->buf + 1, editor->status->len - 1, 0);
    }
    // Search anyway -- empty pattern means repeat the last search.
    editor_search(editor);
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
  search_mode_entered,
  search_mode_key_pressed
};

editing_mode_t *search_mode(void) {
  return &impl;
}
