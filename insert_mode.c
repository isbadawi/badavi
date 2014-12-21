#include "mode.h"

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"

static void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  pos_t *cursor = &editor->window->cursor;
  buffer_t *buffer = editor->window->buffer;
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC:
    editor_status_msg(editor, "");
    editor->mode = &normal_mode;
    return;
  case TB_KEY_BACKSPACE2:
    if (cursor->offset > 0) {
      buf_delete(cursor->line->buf, cursor->offset-- - 1, 1);
    } else if (cursor->line->prev->buf) {
      int prev_len = cursor->line->prev->buf->len;
      buf_insert(
          cursor->line->prev->buf,
          cursor->line->buf->buf,
          prev_len);
      editor_move_up(editor);
      buffer_remove_line(buffer, cursor->line->next);
      cursor->offset = prev_len;
    }
    return;
  case TB_KEY_ENTER:
    buffer_insert_line_after(
        buffer,
        cursor->line->buf->buf + cursor->offset,
        cursor->line);
    buf_delete(
        cursor->line->buf,
        cursor->offset,
        cursor->line->buf->len - cursor->offset);
    editor_move_down(editor);
    cursor->offset = 0;
    return;
  case TB_KEY_SPACE:
    ch = ' ';
    break;
  default:
    ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_insert(cursor->line->buf, s, cursor->offset++);
}

editing_mode_t insert_mode = {insert_mode_key_pressed};
