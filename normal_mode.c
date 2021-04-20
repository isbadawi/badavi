#include "mode.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libgen.h>
#include <limits.h>
#include <termbox.h>

#include "attrs.h"
#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "search.h"
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

void normal_mode_entered(struct editor *editor ATTR_UNUSED) {
  return;
}

void normal_mode_exited(struct editor *editor ATTR_UNUSED) {
  return;
}

void normal_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  if (editor->message->len) {
    if (ev->key == TB_KEY_ENTER) {
      buf_clear(editor->message);
      buf_clear(editor->status);
      editor->status_cursor = 0;
    }
    return;
  }

  if (ev->ch != '0' && isdigit((int) ev->ch)) {
    editor->count = 0;
    while (isdigit((int) ev->ch)) {
      editor->count *= 10;
      editor->count += ev->ch - '0';
      editor_waitkey(editor, ev);
    }
  }

  #define casemod(ch) case ch: if (!editor_try_modify(editor)) { break; }

  switch (ev->key) {
  casemod(TB_KEY_CTRL_R) editor_redo(editor); return;
  case TB_KEY_CTRL_B: window_page_up(editor->window); return;
  case TB_KEY_CTRL_F: window_page_down(editor->window); return;
  case TB_KEY_CTRL_T: editor_tag_stack_prev(editor); return;
  case TB_KEY_CTRL_C:
    editor_status_msg(editor, "Type :q<Enter> to exit badavi");
    return;
  case TB_KEY_CTRL_RSQ_BRACKET: {
    struct buf *word = motion_word_under_cursor(editor->window);
    editor_jump_to_tag(editor, word->buf);
    buf_free(word);
    return;
  }
  case TB_KEY_CTRL_6:
    if (editor->window->alternate_path) {
      editor_open(editor, editor->window->alternate_path);
    } else {
      editor_status_err(editor, "No alternate file");
    }
    return;
  case TB_KEY_ENTER: {
    struct buffer *buffer = editor->window->buffer;
    if (!buffer->directory) {
      return;
    }
    size_t cursor = window_cursor(editor->window);
    struct buf *line = gb_getline(buffer->text, cursor);
    struct buf *path = buf_from_cstr(buffer->path);
    buf_append(path, "/");
    buf_append(path, line->buf);
    editor_open(editor, path->buf);
    buf_free(line);
    buf_free(path);
    return;
  }
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
      editor_set_window(editor, next);
    }

    return;
  }
  }

  struct gapbuf *gb = editor->window->buffer->text;
  size_t cursor = window_cursor(editor->window);
  switch (ev->ch) {
  case 0:
    break;
  case '-': {
    char *path = editor->window->buffer->path;
    if (path && !strcmp(path, "/")) {
      break;
    }

    if (!path) {
      editor_open(editor, ".");
      break;
    }

    bool dir = editor->window->buffer->directory;

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/..", path);
    editor_open(editor, buf);

    snprintf(buf, sizeof(buf), "^%s%s$", basename(path), dir ? "/" : "");
    editor_jump_to_match(editor, buf, 0, SEARCH_FORWARDS);
    break;
  }
  case '"': {
    editor_waitkey(editor, ev);
    char name = (char) tolower((int) ev->ch);
    if (editor_get_register(editor, name)) {
      editor->register_ = name;
    }
    break;
  }
  casemod('i') editor_push_insert_mode(editor, 0); break;
  case 'v': editor_push_visual_mode(editor, VISUAL_MODE_CHARACTERWISE); break;
  case 'V': editor_push_visual_mode(editor, VISUAL_MODE_LINEWISE); break;
  case ':': editor_push_cmdline_mode(editor, ':'); break;
  casemod('p') {
    struct editor_register *r = editor_get_register(editor, editor->register_);
    char *text = r->read(r);
    size_t where = gb_getchar(gb, cursor) == '\n' ? cursor : cursor + 1;
    buffer_start_action_group(editor->window->buffer);
    buffer_do_insert(editor->window->buffer, buf_from_cstr(text), where);
    editor->register_ = '"';
    free(text);
    break;
  }
  casemod('u') editor_undo(editor); break;
  casemod('a') editor_send_keys(editor, "li"); break;
  casemod('I') editor_send_keys(editor, "0i"); break;
  casemod('A') editor_send_keys(editor, "$i"); break;
  casemod('o') editor_send_keys(editor, "A<cr>"); break;
  casemod('O')
    if (cursor < gb->lines->buf[0]) {
      editor_send_keys(editor, "0i<cr><esc>0\"zy^k\"zpi");
    } else {
      editor_send_keys(editor, "ko");
    }
    break;
  case 'x': editor_send_keys(editor, "dl"); break;
  case 'D': editor_send_keys(editor, "d$"); break;
  case 'C': editor_send_keys(editor, "c$"); break;
  casemod('J') editor_join_lines(editor); break;
  case 'g': {
    struct tb_event next;
    editor_waitkey(editor, &next);
    if (next.ch == 'f') {
      struct buf *file = motion_filename_under_cursor(editor->window);
      if (file) {
        char *path = editor_find_in_path(editor, file->buf);
        if (!path) {
          editor_status_err(editor, "Can't find file \"%s\" in path", file->buf);
        } else {
          editor_open(editor, path);
          free(path);
        }
        buf_free(file);
      }
      break;
    }
    editor_push_event(editor, &next);
    ATTR_FALLTHROUGH;
  }
  default: {
    struct motion *motion = motion_get(editor, ev);
    if (motion) {
      window_set_cursor(editor->window, motion_apply(motion, editor));
      break;
    }
    editor_push_operator_pending_mode(editor, ev->ch);
  }
  }
}
