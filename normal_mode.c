#include "mode.h"

#include <ctype.h>

#include "buf.h"
#include "editor.h"

#include <termbox.h>

typedef void (editor_command_t) (editor_t*);
static void repeat(int *count, editor_command_t* cmd, editor_t *editor) {
  int n = *count ? *count : 1;
  for (int i = 0; i < n; ++i) {
    cmd(editor);
  }
  *count = 0;
}

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  static int count = 0;
  static char last = 0;

  if (!isdigit(last)) {
    count = 0;
  }

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
  case '0':
    if (count > 0) {
      count = 10 * count;
    } else {
      cursor->offset = 0;
    }
    break;
  case '1': case '2': case '3': case '4': case '5':
  case '6': case '7': case '8': case '9':
    count = 10 * count + ev->ch - '0';
    break;
  case '$': cursor->offset = cursor->line->buf->len; break;
  case 'h': repeat(&count, editor_move_left, editor); break;
  case 'l': repeat(&count, editor_move_right, editor); break;
  case 'j': repeat(&count, editor_move_down, editor); break;
  case 'k': repeat(&count, editor_move_up, editor); break;
  case 'g':
    if (last == 'g') {
      cursor->line = editor->window->buffer->head->next;
      cursor->offset = 0;
    }
    break;
  case 'G': {
    int n = editor->window->buffer->nlines;
    repeat(&n, editor_move_down, editor);
    break;
  }
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
  last = ev->ch;
}

editing_mode_t normal_mode = {normal_mode_key_pressed};
