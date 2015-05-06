#include "mode.h"

#include <ctype.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "motion.h"
#include "mode.h"

static int is_last_line(gapbuf_t *gb, int pos) {
  return pos > gb_size(gb) - gb->lines->buf[gb->lines->len - 1];
}

static void normal_mode_entered(editor_t *editor) {
  if (editor->motion) {
    editor->window->cursor = motion_apply(editor);
  }
}

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  if (ev->key == TB_KEY_CTRL_C) {
    editor_status_msg(editor, "Type :q<Enter> to exit badavi");
    return;
  }

  if (ev->key == TB_KEY_CTRL_R) {
    editor_redo(editor);
    return;
  }

  if (ev->ch != '0' && isdigit(ev->ch)) {
    editor_push_mode(editor, digit_mode());
    editor_handle_key_press(editor, ev);
    return;
  }

  gapbuf_t *gb = editor->window->buffer->text;
  int *cursor = &editor->window->cursor;
  switch (ev->ch) {
  case '"':
    editor_push_mode(editor, quote_mode());
    break;
  case 'i':
    editor_push_mode(editor, insert_mode());
    break;
  case ':':
    editor_push_mode(editor, command_mode());
    break;
  case '/':
    editor_push_mode(editor, search_mode());
    break;
  case 'p': {
    buf_t *reg = editor_get_register(editor, editor->register_);
    int where = gb_getchar(gb, *cursor) == '\n' ? *cursor : *cursor + 1;
    gb_putstring(gb, reg->buf, reg->len, where);
    *cursor = where + reg->len - 1;
    edit_action_t action = {
      .type = EDIT_ACTION_INSERT,
      .pos = where,
      .buf = buf_copy(reg)
    };
    editor_start_action_group(editor);
    editor_add_action(editor, action);
    editor->register_ = '"';
    break;
  }
  case 'u': editor_undo(editor); break;
  case 'n': editor_search(editor); break;
  case 'a': editor_send_keys(editor, "li"); break;
  case 'I': editor_send_keys(editor, "0i"); break;
  case 'A': editor_send_keys(editor, "$i"); break;
  case 'o': editor_send_keys(editor, "A<cr>"); break;
  case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
  case 'x': editor_send_keys(editor, "dl"); break;
  case 'D': editor_send_keys(editor, "d$"); break;
  case 'C': editor_send_keys(editor, "c$"); break;
  case 'J':
    if (!is_last_line(gb, *cursor)) {
      editor_send_keys(editor, "A <esc>jI<bs><esc>");
    }
    break;
  default: {
    editing_mode_t *mode = operator_pending_mode(ev->ch);
    if (mode) {
      editor_push_mode(editor, mode);
    } else {
      editor_push_mode(editor, motion_mode());
      editor_handle_key_press(editor, ev);
    }
  }
  }
}

static editing_mode_t impl = {
  normal_mode_entered,
  normal_mode_key_pressed
};

editing_mode_t *normal_mode(void) {
  return &impl;
}
