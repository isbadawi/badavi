#include "mode.h"

#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "motion.h"

static int is_last_line(gapbuf_t *gb, int pos) {
  return pos > gb_size(gb) - gb->lines->buf[gb->lines->len - 1];
}

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  static motion_t motion = {NULL, 0, 0};

  int rc = motion_key(&motion, ev);
  if (rc == 1) {
    editor->window->cursor = motion_apply(&motion, editor->window);
    return;
  } else if (rc == 0) {
    return;
  }

  switch (ev->ch) {
  case 'i':
    editor_status_msg(editor, "-- INSERT --");
    editor->mode = insert_mode();
    break;
  case ':':
    editor_status_msg(editor, ":");
    editor->mode = command_mode();
    break;
  case 'p': {
    buf_t *reg = editor_get_register(editor, '"');
    gapbuf_t *gb = editor->window->buffer->text;
    int *cursor = &editor->window->cursor;
    int where = gb_getchar(gb, *cursor) == '\n' ? *cursor : *cursor + 1;
    gb_putstring(gb, reg->buf, reg->len, where);
    *cursor = where + reg->len - 1;
    break;
  }
  case 'a': editor_send_keys(editor, "li"); break;
  case 'I': editor_send_keys(editor, "0i"); break;
  case 'A': editor_send_keys(editor, "$i"); break;
  case 'o': editor_send_keys(editor, "A<cr>"); break;
  case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
  case 'x': editor_send_keys(editor, "dl"); break;
  case 'D': editor_send_keys(editor, "d$"); break;
  case 'C': editor_send_keys(editor, "c$"); break;
  case 'J':
    if (!is_last_line(editor->window->buffer->text, editor->window->cursor)) {
      editor_send_keys(editor, "A <esc>jI<bs><esc>");
    }
    break;
  default: {
    editing_mode_t *mode = operator_pending_mode(ev->ch);
    if (mode) {
      editor->mode = mode;
    }
  }
  }
}

static editing_mode_t impl = {normal_mode_key_pressed};

editing_mode_t *normal_mode(void) {
  return &impl;
}
