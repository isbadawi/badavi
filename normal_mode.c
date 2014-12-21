#include "mode.h"

#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "motion.h"

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  static motion_t motion = {NULL, 0, 0};

  int rc = motion_key(&motion, ev);
  if (rc == 1) {
    editor->window->cursor = motion_apply(&motion, editor->window);
    return;
  } else if (rc == 0) {
    return;
  }

  pos_t *cursor = &editor->window->cursor;
  switch (ev->ch) {
  case 'i':
    editor_status_msg(editor, "-- INSERT --");
    editor->mode = insert_mode();
    break;
  case ':':
    editor_status_msg(editor, ":");
    editor->mode = command_mode();
    break;
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

static editing_mode_t impl = {normal_mode_key_pressed};

editing_mode_t *normal_mode(void) {
  return &impl;
}
