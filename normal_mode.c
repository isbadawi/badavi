#include "mode.h"

#include "buf.h"
#include "editor.h"

#include <termbox.h>

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  pos_t *cursor = &editor->window->cursor;
  switch (ev->ch) {
    case 'i':
      editor_status_msg(editor, "-- INSERT --");
      editor->mode = &insert_mode;
      break;
    case ':':
      editor_status_msg(editor, ":");
      editor->mode = &command_mode;
      break;
    case '0': cursor->offset = 0; break;
    case '$': cursor->offset = cursor->line->buf->len; break;
    case 'h': editor_move_left(editor); break;
    case 'l': editor_move_right(editor); break;
    case 'j': editor_move_down(editor); break;
    case 'k': editor_move_up(editor); break;
    case 'a': editor_send_keys(editor, "li"); break;
    case 'I': editor_send_keys(editor, "0i"); break;
    case 'A': editor_send_keys(editor, "$i"); break;
    case 'o': editor_send_keys(editor, "A<cr>"); break;
    case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
    case 'x': editor_send_keys(editor, "a<bs><esc>"); break;
    case 'J':
      if (cursor->line->next) {
        editor_send_keys(editor, "A <esc>jI<bs><esc>");
      }
      break;
    case 'D':
      buf_delete(
          cursor->line->buf,
          cursor->offset,
          cursor->line->buf->len - cursor->offset);
      break;
  }
}

editing_mode_t normal_mode = {normal_mode_key_pressed};
