#include "mode.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "tags.h"
#include "util.h"
#include "window.h"

static bool is_last_line(struct gapbuf *gb, size_t pos) {
  return pos > gb_size(gb) - gb->lines->buf[gb->lines->len - 1];
}

static void editor_join_lines(struct editor *editor) {
  struct buffer *buffer = editor->window->buffer;
  struct gapbuf *gb = buffer->text;
  size_t cursor = window_cursor(editor->window);
  if (is_last_line(gb, cursor)) {
    return;
  }

  size_t newline = gb_indexof(gb, '\n', cursor);
  size_t next_newline = newline + 1;
  if (gb_getchar(gb, next_newline) != '\n') {
    next_newline = gb_indexof(gb, '\n',  next_newline);
  }
  size_t first_non_blank;
  for (first_non_blank = newline + 1;
       first_non_blank < next_newline &&
       isspace(gb_getchar(gb, first_non_blank));
       first_non_blank++) {}

  buffer_start_action_group(buffer);
  buffer_do_delete(buffer, first_non_blank - newline, newline);
  buffer_do_insert(buffer, buf_from_char(' '), newline);
  window_set_cursor(editor->window, newline);
}

static void normal_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  if (ev->ch != '0' && isdigit((int) ev->ch)) {
    editor->count = 0;
    while (isdigit((int) ev->ch)) {
      editor->count *= 10;
      editor->count += ev->ch - '0';
      editor_waitkey(editor, ev);
    }
  }

  switch (ev->key) {
  case TB_KEY_CTRL_C:
    editor_status_msg(editor, "Type :q<Enter> to exit badavi");
    return;
  case TB_KEY_CTRL_R:
    editor_redo(editor);
    return;
  case TB_KEY_CTRL_RSQ_BRACKET: {
    struct buf *word = motion_word_under_cursor(editor->window);
    editor_jump_to_tag(editor, word->buf);
    buf_free(word);
    return;
  }
  case TB_KEY_CTRL_T:
    editor_tag_stack_prev(editor);
    return;
  case TB_KEY_CTRL_W: {
    editor_waitkey(editor, ev);
    struct window *next = NULL;
    switch (ev->key) {
    case TB_KEY_CTRL_H: next = window_left(editor->window); break;
    case TB_KEY_CTRL_L: next = window_right(editor->window); break;
    case TB_KEY_CTRL_K: next = window_up(editor->window); break;
    case TB_KEY_CTRL_J: next = window_down(editor->window); break;
    }

    int count = max(1, (int)editor->count);

    switch (ev->ch) {
    case 'h': next = window_left(editor->window); break;
    case 'l': next = window_right(editor->window); break;
    case 'k': next = window_up(editor->window); break;
    case 'j': next = window_down(editor->window); break;
    case '<':
      window_resize(editor->window, -count, 0);
      editor->count = 0;
      break;
    case '>':
      window_resize(editor->window, count, 0);
      editor->count = 0;
      break;
    case '-':
      window_resize(editor->window, 0, -count);
      editor->count = 0;
      break;
    case '+':
      window_resize(editor->window, 0, count);
      editor->count = 0;
      break;
    case '=':
      window_equalize(editor->window, WINDOW_SPLIT_HORIZONTAL);
      window_equalize(editor->window, WINDOW_SPLIT_VERTICAL);
      break;
    }

    if (next) {
      editor->window = next;
    }

    return;
  }
  }

  struct gapbuf *gb = editor->window->buffer->text;
  size_t cursor = window_cursor(editor->window);
  switch (ev->ch) {
  case 0:
    break;
  case '"': {
    editor_waitkey(editor, ev);
    char name = (char) tolower((int) ev->ch);
    if (editor_get_register(editor, name)) {
      editor->register_ = name;
    }
    break;
  }
  case 'i':
    editor_push_mode(editor, insert_mode());
    break;
  case 'v':
    editor_push_mode(editor, visual_mode(VISUAL_MODE_CHARACTERWISE));
    break;
  case 'V':
    editor_push_mode(editor, visual_mode(VISUAL_MODE_LINEWISE));
    break;
  case ':':
    editor_push_mode(editor, command_mode());
    break;
  case 'p': {
    struct buf *reg = editor_get_register(editor, editor->register_);
    size_t where = gb_getchar(gb, cursor) == '\n' ? cursor : cursor + 1;
    buffer_start_action_group(editor->window->buffer);
    buffer_do_insert(editor->window->buffer, buf_copy(reg), where);
    editor->register_ = '"';
    break;
  }
  case 'u': editor_undo(editor); break;
  case 'a': editor_send_keys(editor, "li"); break;
  case 'I': editor_send_keys(editor, "0i"); break;
  case 'A': editor_send_keys(editor, "$i"); break;
  case 'o': editor_send_keys(editor, "A<cr>"); break;
  case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
  case 'x': editor_send_keys(editor, "dl"); break;
  case 'D': editor_send_keys(editor, "d$"); break;
  case 'C': editor_send_keys(editor, "c$"); break;
  case 'J':
    editor_join_lines(editor);
    break;
  default: {
    struct motion *motion = motion_get(editor, ev);
    if (motion) {
      window_set_cursor(editor->window, motion_apply(motion, editor));
      break;
    }
    struct editing_mode *mode = operator_pending_mode((char) ev->ch);
    if (mode) {
      editor_push_mode(editor, mode);
    }
  }
  }
}

static struct editing_mode impl = {
  .entered = NULL,
  .exited = NULL,
  .key_pressed = normal_mode_key_pressed,
  .parent = NULL
};

struct editing_mode *normal_mode(void) {
  return &impl;
}
