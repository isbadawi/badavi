#include "motion.h"

#include <ctype.h>
#include <string.h>

#include <termbox.h>

#include "util.h"

static int is_line_start(gapbuf_t *gb, int pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static int is_line_end(gapbuf_t *gb, int pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos + 1) == '\n';
}

static int is_first_line(gapbuf_t *gb, int pos) {
  return pos < gb->lines->buf[0];
}

static int is_last_line(gapbuf_t *gb, int pos) {
  return pos > gb_size(gb) - gb->lines->buf[gb->lines->len - 1];
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
  return gb_linecol_to_pos(gb, y - 1, min(x, gb->lines->buf[y - 2]));
}

static int down(int pos, window_t *window) {
  if (is_last_line(window->buffer->text, pos)) {
    return pos;
  }
  gapbuf_t *gb = window->buffer->text;
  int x, y;
  gb_pos_to_linecol(gb, pos, &y, &x);
  return gb_linecol_to_pos(gb, y + 1, min(x, gb->lines->buf[y]));
}

static int line_start(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  return gb_lastindexof(gb, '\n', pos) + 1;
}

static int line_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  return gb_indexof(gb, '\n', pos) - 1;
}

static int buffer_top(int pos, window_t *window) {
  return line_start(window->top, window);
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
  if (pos == 0) {
    return 1;
  }
  gapbuf_t *gb = window->buffer->text;
  char this = gb_getchar(gb, pos);
  char last = gb_getchar(gb, pos - 1);
  return !isspace(this) && is_word_char(this) + is_word_char(last) == 1;
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
  if (pos == gb_size(gb)) {
    return 1;
  }
  char this = gb_getchar(gb, pos);
  char next = gb_getchar(gb, pos + 1);
  return !isspace(this) && is_word_char(this) + is_word_char(next) == 1;
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
  if (pos < size) {
    do {
      ++pos;
    } while (!pred(pos, window) && pos < size);
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
  char this = gb_getchar(gb, pos);
  char next = gb_getchar(gb, pos + 1);
  return this == '\n' && next != '\n';
}

static int is_paragraph_end(int pos, window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  if (pos == gb_size(gb)) {
    return 1;
  }
  char this = gb_getchar(gb, pos);
  char last = gb_getchar(gb, pos - 1);
  return this == '\n' && last != '\n';
}

static int paragraph_start(int pos, window_t *window) {
  return prev_until(pos, window, is_paragraph_start);
}

static int paragraph_end(int pos, window_t *window) {
  return next_until(pos, window, is_paragraph_end);
}

static motion_t motion_table[] = {
  {"h", left},
  {"j", down},
  {"k", up},
  {"l", right},
  {"0", line_start},
  {"$", line_end},
  {"^", first_non_blank},
  {"g_", last_non_blank},
  {"{", paragraph_start},
  {"}", paragraph_end},
  {"b", prev_word_start},
  {"B", prev_WORD_start},
  {"w", next_word_start},
  {"W", next_WORD_start},
  {"e", next_word_end},
  {"E", next_WORD_end},
  {"ge", prev_word_end},
  {"gE", prev_WORD_end},
  {"G", down}, // See motion_apply...
  {"gg", buffer_top},
  // TODO(isbadawi): What about {t,f,T,F}{char}?
  {NULL, NULL}
};

static motion_t *motion_find(char *name) {
  for (int i = 0; motion_table[i].name != NULL; ++i) {
    if (!strcmp(motion_table[i].name, name)) {
      return &motion_table[i];
    }
  }
  return NULL;
}

static int motion_exists_with_prefix(char *name) {
  for (int i = 0; motion_table[i].name != NULL; ++i) {
    if (!strncmp(motion_table[i].name, name, strlen(name))) {
      return 1;
    }
  }
  return 0;
}

int motion_key(motion_t *motion, struct tb_event *ev) {
  if (!isdigit(motion->last)) {
    motion->count = 0;
  }

  if (isdigit(ev->ch) && !(motion->count == 0 && ev->ch == '0')) {
    motion->count *= 10;
    motion->count += ev->ch - '0';
    motion->last = ev->ch;
    return 0;
  }

  motion_t *m = NULL;
  char name[3];
  if (motion->last) {
    name[0] = motion->last;
    name[1] = ev->ch;
    name[2] = '\0';
    m = motion_find(name);
  }
  if (!m) {
    name[0] = ev->ch;
    name[1] = '\0';
    m = motion_find(name);
  }
  if (m) {
    motion->name = m->name;
    motion->op = m->op;
    return 1;
  }
  if (motion_exists_with_prefix(name)) {
    motion->last = ev->ch;
    return 0;
  }
  return -1;
}

int motion_apply(motion_t *motion, window_t *window) {
  int result = window->cursor;
  int n = motion->count ? motion->count : 1;

  // TODO(isbadawi): This is a hack to make G work correctly.
  if (!strcmp(motion->name, "G")) {
    n = motion->count ? motion->count - 1 : window->buffer->text->lines->len - 1;
    result = buffer_top(result, window);
  }

  for (int i = 0; i < n; ++i) {
    result = motion->op(result, window);
  }

  motion->op = NULL;
  motion->count = 0;
  motion->last = 0;
  return result;
}
