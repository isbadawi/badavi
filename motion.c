#include "motion.h"

#include <ctype.h>
#include <string.h>

#include <termbox.h>

#include "editor.h"
#include "mode.h"
#include "util.h"

static int is_line_start(gapbuf_t *gb, int pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static int is_line_end(gapbuf_t *gb, int pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos) == '\n';
}

static int is_first_line(gapbuf_t *gb, int pos) {
  return pos < gb->lines->buf[0];
}

static int is_last_line(gapbuf_t *gb, int pos) {
  return pos >= gb_size(gb) - 1 - gb->lines->buf[gb->lines->len - 1];
}

static int is_blank_line(gapbuf_t *gb, int pos) {
  return gb_getchar(gb, pos) == '\n' &&
    (pos == 0 || gb_getchar(gb, pos - 1) == '\n');
}

static int left(int pos, window_t *window) {
  return is_line_start(window->buffer->text, pos) ? pos : pos - 1;
}

static int right(int pos, window_t *window) {
  return is_line_end(window->buffer->text, pos) ? pos : pos + 1;
}

static int up(int pos, window_t *window) {
  if (is_first_line(window->buffer->text, pos)) {
    return pos;
  }
  gapbuf_t *gb = window->buffer->text;
  int x, y;
  gb_pos_to_linecol(gb, pos, &y, &x);
  return gb_linecol_to_pos(gb, y - 1, min(x, gb->lines->buf[y - 1]));
}

static int down(int pos, window_t *window) {
  if (is_last_line(window->buffer->text, pos)) {
    return pos;
  }
  gapbuf_t *gb = window->buffer->text;
  int x, y;
  gb_pos_to_linecol(gb, pos, &y, &x);
  return gb_linecol_to_pos(gb, y + 1, min(x, gb->lines->buf[y + 1]));
}

static int line_start(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (is_line_start(gb, pos)) {
    return pos;
  }
  return gb_lastindexof(gb, '\n', pos - 1) + 1;
}

static int line_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (is_line_end(gb, pos)) {
    return pos;
  }
  return gb_indexof(gb, '\n', pos);
}

static int buffer_top(int pos, window_t *window) {
  return 0;
}

static int first_non_blank(int pos, window_t *window) {
  int start = line_start(pos, window);
  int end = line_end(pos, window);
  gapbuf_t *gb = window->buffer->text;
  while (start < end && isspace(gb_getchar(gb, start))) {
    start++;
  }
  return start;
}

static int last_non_blank(int pos, window_t *window) {
  int start = line_start(pos, window);
  int end = line_end(pos, window);
  gapbuf_t *gb = window->buffer->text;
  while (end > start && isspace(gb_getchar(gb, end))) {
    end--;
  }
  return end;
}

static int is_word_char(char c) {
  return isalnum(c) || c == '_';
}

static int is_word_start(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (is_line_start(gb, pos)) {
    return 1;
  }
  char this = gb_getchar(gb, pos);
  char last = gb_getchar(gb, pos - 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(last) == 1);
}

static int is_WORD_start(int pos, window_t *window) {
  if (pos == 0) {
    return 1;
  }
  gapbuf_t *gb = window->buffer->text;
  char this = gb_getchar(gb, pos);
  char last = gb_getchar(gb, pos - 1);
  return !isspace(this) && isspace(last);
}

static int is_word_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (pos == gb_size(gb) - 1 || is_blank_line(gb, pos)) {
    return 1;
  }
  char this = gb_getchar(gb, pos);
  char next = gb_getchar(gb, pos + 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(next) == 1);
}

static int is_WORD_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (pos == gb_size(gb)) {
    return 1;
  }
  char this = gb_getchar(gb, pos);
  char next = gb_getchar(gb, pos + 1);
  return !isspace(this) && isspace(next);
}

static int prev_until(int pos, window_t *window, motion_op_t *pred) {
  if (pos > 0) {
    do {
      --pos;
    } while (!pred(pos, window) && pos > 0);
  }
  return pos;
}

static int next_until(int pos, window_t *window, motion_op_t *pred) {
  int size = gb_size(window->buffer->text);
  if (pos < size - 1) {
    do {
      ++pos;
    } while (!pred(pos, window) && pos < size - 1);
  }
  return pos;
}

static int next_word_start(int pos, window_t *window) {
  return next_until(pos, window, is_word_start);
}

static int next_WORD_start(int pos, window_t *window) {
  return next_until(pos, window, is_WORD_start);
}

static int prev_word_start(int pos, window_t *window) {
  return prev_until(pos, window, is_word_start);
}

static int prev_WORD_start(int pos, window_t *window) {
  return prev_until(pos, window, is_WORD_start);
}

static int next_word_end(int pos, window_t *window) {
  return next_until(pos, window, is_word_end);
}

static int next_WORD_end(int pos, window_t *window) {
  return next_until(pos, window, is_WORD_end);
}

static int prev_word_end(int pos, window_t *window) {
  return prev_until(pos, window, is_word_end);
}

static int prev_WORD_end(int pos, window_t *window) {
  return prev_until(pos, window, is_WORD_end);
}

static int is_paragraph_start(int pos, window_t *window) {
  if (pos == 0) {
    return 1;
  }
  gapbuf_t *gb = window->buffer->text;
  return is_blank_line(gb, pos) && !is_blank_line(gb, pos + 1);
}

static int is_paragraph_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (pos == gb_size(gb) - 1) {
    return 1;
  }
  return is_blank_line(gb, pos) && !is_blank_line(gb, pos - 1);
}

static int paragraph_start(int pos, window_t *window) {
  return prev_until(pos, window, is_paragraph_start);
}

static int paragraph_end(int pos, window_t *window) {
  return next_until(pos, window, is_paragraph_end);
}

static motion_t motion_table[] = {
  {'h', left},
  {'j', down},
  {'k', up},
  {'l', right},
  {'0', line_start},
  {'$', line_end},
  {'^', first_non_blank},
  {'{', paragraph_start},
  {'}', paragraph_end},
  {'b', prev_word_start},
  {'B', prev_WORD_start},
  {'w', next_word_start},
  {'W', next_WORD_start},
  {'e', next_word_end},
  {'E', next_WORD_end},
  {'G', down}, // See motion_apply...
  // TODO(isbadawi): What about {t,f,T,F}{char}?
  {-1, NULL}
};

static motion_t g_motion_table[] = {
  {'_', last_non_blank},
  {'e', prev_word_end},
  {'E', prev_WORD_end},
  {'g', buffer_top},
  {-1, NULL}
};

static motion_t *motion_find(motion_t *table, char name) {
  for (int i = 0; table[i].name != -1; ++i) {
    if (table[i].name == name) {
      return &table[i];
    }
  }
  return NULL;
}

static void no_op(editor_t *editor) {
}

static void g_pressed(editor_t *editor, struct tb_event *ev) {
  motion_t *motion = motion_find(g_motion_table, ev->ch);
  if (motion) {
    editor->motion = motion;
  }
  editor_pop_mode(editor);
  editor_pop_mode(editor);
}

static editing_mode_t g_mode = {
  no_op,
  g_pressed
};

void key_pressed(editor_t *editor, struct tb_event *ev) {
  if (ev->ch == 'g') {
    editor_push_mode(editor, &g_mode);
    return;
  }
  motion_t *motion = motion_find(motion_table, ev->ch);
  if (motion) {
    editor->motion = motion;
  }
  editor_pop_mode(editor);
  return;
}

static editing_mode_t impl = {
  no_op,
  key_pressed
};

editing_mode_t *motion_mode(void) {
  return &impl;
}

int motion_apply(editor_t *editor) {
  motion_t *motion = editor->motion;
  int result = editor->window->cursor;
  int n = editor->count ? editor->count : 1;

  // TODO(isbadawi): This is a hack to make G work correctly.
  if (motion->name == 'G') {
    n = editor->count ? editor->count - 1 : gb_nlines(editor->window->buffer->text) - 1;
    result = buffer_top(result, editor->window);
  }

  for (int i = 0; i < n; ++i) {
    result = motion->op(result, editor->window);
  }

  editor->count = 0;
  editor->motion = NULL;
  return result;
}
