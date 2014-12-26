#include "mode.h"

#include <termbox.h>

#include "buffer.h"
#include "gap.h"
#include "editor.h"

static void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  buffer_t *buffer = editor->window->buffer;
  gapbuf_t *gb = buffer->text;
  int *cursor = &editor->window->cursor;
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC:
    editor_status_msg(editor, "");
    editor->mode = normal_mode();
    return;
  case TB_KEY_BACKSPACE2:
    if (cursor > 0) {
      gb_del(gb, 1, (*cursor)--);
      buffer->dirty = 1;
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  default: ch = ev->ch; break;
  }
  gb_putchar(gb, ch, (*cursor)++);
  buffer->dirty = 1;
}

editing_mode_t impl = {insert_mode_key_pressed};

editing_mode_t *insert_mode(void) {
  return &impl;
}
