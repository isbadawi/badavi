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
#include "search.h"
#include "tags.h"
#include "window.h"

static bool is_last_line(struct gapbuf_t *gb, size_t pos) {
  return pos > gb_size(gb) - gb->lines->buf[gb->lines->len - 1];
}

static void normal_mode_key_pressed(struct editor_t* editor, struct tb_event* ev) {
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
    char word[256];
    motion_word_under_cursor(editor->window, word);
    editor_jump_to_tag(editor, word);
    return;
  }
  case TB_KEY_CTRL_T:
    editor_tag_stack_prev(editor);
    return;
  case TB_KEY_CTRL_H: {
    struct window_t *left = editor_left_window(editor, editor->window);
    if (left) {
      editor->window = left;
    }
    return;
  }
  case TB_KEY_CTRL_L: {
    struct window_t *right = editor_right_window(editor, editor->window);
    if (right) {
      editor->window = right;
    }
    return;
  }
  }

  struct gapbuf_t *gb = editor->window->buffer->text;
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
  case ':':
    editor_push_mode(editor, command_mode());
    break;
  case '/': case '?':
    editor_push_mode(editor, search_mode((char) ev->ch));
    break;
  case '*': case '#': {
    char word[256];
    size_t start = motion_word_under_cursor(editor->window, word);
    char pattern[256];
    snprintf(pattern, 256, "[[:<:]]%s[[:>:]]", word);
    if (ev->ch == '*') {
      editor_search(editor, pattern, cursor, SEARCH_FORWARDS);
    } else {
      editor_search(editor, pattern, start, SEARCH_BACKWARDS);
    }
    break;
  }
  case 'p': {
    struct buf_t *reg = editor_get_register(editor, editor->register_);
    size_t where = gb_getchar(gb, cursor) == '\n' ? cursor : cursor + 1;
    buffer_start_action_group(editor->window->buffer);
    buffer_do_insert(editor->window->buffer, buf_copy(reg), where);
    editor->register_ = '"';
    break;
  }
  case 'u': editor_undo(editor); break;
  case 'n': editor_search(editor, NULL, cursor, SEARCH_FORWARDS); break;
  case 'N': editor_search(editor, NULL, cursor, SEARCH_BACKWARDS); break;
  case 'a': editor_send_keys(editor, "li"); break;
  case 'I': editor_send_keys(editor, "0i"); break;
  case 'A': editor_send_keys(editor, "$i"); break;
  case 'o': editor_send_keys(editor, "A<cr>"); break;
  case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
  case 'x': editor_send_keys(editor, "dl"); break;
  case 'D': editor_send_keys(editor, "d$"); break;
  case 'C': editor_send_keys(editor, "c$"); break;
  case 'J':
    if (!is_last_line(gb, cursor)) {
      editor_send_keys(editor, "A <esc>jI<bs><esc>");
    }
    break;
  default: {
    struct motion_t *motion = motion_get(editor, ev);
    if (motion) {
      window_set_cursor(editor->window ,motion_apply(motion, editor));
      break;
    }
    struct editing_mode_t *mode = operator_pending_mode((char) ev->ch);
    if (mode) {
      editor_push_mode(editor, mode);
    }
  }
  }
}

static struct editing_mode_t impl = {
  .entered = NULL,
  .exited = NULL,
  .key_pressed = normal_mode_key_pressed,
  .parent = NULL
};

struct editing_mode_t *normal_mode(void) {
  return &impl;
}
