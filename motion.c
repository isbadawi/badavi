#include "motion.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "search.h"
#include "window.h"
#include "util.h"

static bool is_line_start(struct gapbuf *gb, size_t pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static bool is_line_end(struct gapbuf *gb, size_t pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos) == '\n';
}

static bool is_blank_line(struct gapbuf *gb, size_t pos) {
  return gb_getchar(gb, pos) == '\n' && is_line_start(gb, pos);
}

static size_t left(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  return is_line_start(gb, ctx.pos) ? ctx.pos : gb_utf8prev(gb, ctx.pos);
}

static size_t right(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  return is_line_end(gb, ctx.pos) ? ctx.pos : gb_utf8next(gb, ctx.pos);
}

static size_t goto_line(struct motion_context ctx) {
  struct gapbuf *gb = ctx.editor->window->buffer->text;
  int line = gb_nlines(gb) - 1;
  if (ctx.editor->count) {
    line = min((int)ctx.editor->count - 1, line);
  }
  return gb_linecol_to_pos(gb, line, 0);
}

static size_t up(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  size_t x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  int dy = ctx.editor->count ? ctx.editor->count : 1;
  int line = max((int)y - dy, 0);
  return gb_linecol_to_pos(gb, line, min(x, gb->lines->buf[line]));
}

static size_t down(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  int nlines = gb_nlines(gb);
  size_t x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  int dy = ctx.editor->count ? ctx.editor->count : 1;
  int line = min((int)y + dy, nlines - 1);
  return gb_linecol_to_pos(gb, line, min(x, gb->lines->buf[line]));
}

static size_t line_start(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }
  return (size_t) (gb_lastindexof(gb, '\n', ctx.pos - 1) + 1);
}

static size_t line_end(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }
  return gb_indexof(gb, '\n', ctx.pos);
}

static size_t buffer_top(struct motion_context ctx ATTR_UNUSED) {
  return 0;
}

static size_t first_non_blank(struct motion_context ctx) {
  size_t start = line_start(ctx);
  size_t end = line_end(ctx);
  struct gapbuf *gb = ctx.window->buffer->text;
  while (start < end && isspace(gb_getchar(gb, start))) {
    start++;
  }
  return start;
}

static size_t last_non_blank(struct motion_context ctx) {
  size_t start = line_start(ctx);
  size_t end = line_end(ctx);
  struct gapbuf *gb = ctx.window->buffer->text;
  while (end > start && isspace(gb_getchar(gb, end))) {
    end--;
  }
  return end;
}

static bool is_word_char(char c) {
  // FIXME(ibadawi): Need a unicode-aware version of isalnum().
  // As a quick hack to make word motions work, treat non-ascii as letters.
  return isalnum(c) || c == '_' || tb_utf8_char_length(c) > 1;
}

static bool is_word_start(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, gb_utf8prev(gb, ctx.pos));
  return !isspace(this) && (is_word_char(this) + is_word_char(last) == 1);
}

static bool is_WORD_start(struct motion_context ctx) {
  if (ctx.pos == 0) {
    return true;
  }
  struct gapbuf *gb = ctx.window->buffer->text;
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, gb_utf8prev(gb, ctx.pos));
  return !isspace(this) && isspace(last);
}

static bool is_word_end(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1 || is_blank_line(gb, ctx.pos)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, gb_utf8next(gb, ctx.pos));
  return !isspace(this) && (is_word_char(this) + is_word_char(next) == 1);
}

static bool is_WORD_end(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, gb_utf8next(gb, ctx.pos));
  return !isspace(this) && isspace(next);
}

static size_t prev_until(struct motion_context ctx, bool (*pred)(struct motion_context)) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (ctx.pos > 0) {
    do {
      ctx.pos = gb_utf8prev(gb, ctx.pos);
    } while (!pred(ctx) && ctx.pos > 0);
  }
  return ctx.pos;
}

static size_t next_until(struct motion_context ctx, bool (*pred)(struct motion_context)) {
  struct gapbuf *gb = ctx.window->buffer->text;
  size_t size = gb_size(ctx.window->buffer->text);
  if (ctx.pos < size - 1) {
    do {
      ctx.pos = gb_utf8next(gb, ctx.pos);
    } while (!pred(ctx) && ctx.pos < size - 1);
  }
  return ctx.pos;
}

static size_t next_word_start(struct motion_context ctx) {
  return next_until(ctx, is_word_start);
}

static size_t next_WORD_start(struct motion_context ctx) {
  return next_until(ctx, is_WORD_start);
}

static size_t prev_word_start(struct motion_context ctx) {
  return prev_until(ctx, is_word_start);
}

static size_t prev_WORD_start(struct motion_context ctx) {
  return prev_until(ctx, is_WORD_start);
}

static size_t next_word_end(struct motion_context ctx) {
  return next_until(ctx, is_word_end);
}

static size_t next_WORD_end(struct motion_context ctx) {
  return next_until(ctx, is_WORD_end);
}

static size_t prev_word_end(struct motion_context ctx) {
  return prev_until(ctx, is_word_end);
}

static size_t prev_WORD_end(struct motion_context ctx) {
  return prev_until(ctx, is_WORD_end);
}

static bool is_paragraph_start(struct motion_context ctx) {
  if (ctx.pos == 0) {
    return true;
  }
  struct gapbuf *gb = ctx.window->buffer->text;
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos + 1);
}

static bool is_paragraph_end(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1) {
    return true;
  }
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos - 1);
}

static size_t paragraph_start(struct motion_context ctx) {
  return prev_until(ctx, is_paragraph_start);
}

static size_t paragraph_end(struct motion_context ctx) {
  return next_until(ctx, is_paragraph_end);
}

static size_t till_forward(struct motion_context ctx, bool inclusive) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = editor_getchar(ctx.editor);
  size_t newline = gb_indexof(gb, '\n', ctx.pos + 1);
  size_t target = gb_indexof(gb, arg, ctx.pos + 1);
  if (target < newline) {
    return target - (inclusive ? 0 : 1);
  }
  return ctx.pos;
}

static size_t till_backward(struct motion_context ctx, bool inclusive) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = editor_getchar(ctx.editor);
  ssize_t newline = gb_lastindexof(gb, '\n', ctx.pos - 1);
  ssize_t target = gb_lastindexof(gb, arg, ctx.pos - 1);
  if (target > newline) {
    return (size_t) target + (inclusive ? 0 : 1);
  }
  return ctx.pos;
}

static size_t till_forward_inclusive(struct motion_context ctx) {
  return till_forward(ctx, true);
}

static size_t till_forward_exclusive(struct motion_context ctx) {
  return till_forward(ctx, false);
}

static size_t till_backward_inclusive(struct motion_context ctx) {
  return till_backward(ctx, true);
}

static size_t till_backward_exclusive(struct motion_context ctx) {
  return till_backward(ctx, false);
}

static size_t search_motion(struct motion_context ctx, char direction) {
  editor_push_cmdline_mode(ctx.editor, direction);
  struct editing_mode *mode = ctx.editor->mode;
  editor_draw(ctx.editor);
  while (ctx.editor->mode == mode) {
    struct tb_event event;
    editor_waitkey(ctx.editor, &event);
    editor_handle_key_press(ctx.editor, &event);
    editor_draw(ctx.editor);
  }
  size_t result = window_cursor(ctx.editor->window);
  window_set_cursor(ctx.editor->window, ctx.pos);
  return result;
}

static size_t forward_search(struct motion_context ctx) {
  return search_motion(ctx, '/');
}

static size_t backward_search(struct motion_context ctx) {
  return search_motion(ctx, '?');
}

static size_t search_cycle(struct motion_context ctx,
                           enum search_direction direction) {
  struct region match;
  if (editor_search(ctx.editor, NULL, ctx.pos, direction, &match)) {
    return match.start;
  }
  return ctx.pos;
}

static size_t search_next(struct motion_context ctx) {
  return search_cycle(ctx, SEARCH_FORWARDS);
}

static size_t search_prev(struct motion_context ctx) {
  return search_cycle(ctx, SEARCH_BACKWARDS);
}

static size_t word_under_cursor(struct motion_context ctx, size_t start,
                                enum search_direction direction) {
  struct buf *word = motion_word_under_cursor(ctx.editor->window);
  struct buf *pattern = buf_create(1);
  buf_printf(pattern, "[[:<:]]%s[[:>:]]", word->buf);
  history_add_item(&ctx.editor->search_history, pattern->buf);
  struct region match;
  bool found = editor_search(ctx.editor, pattern->buf, start, direction, &match);
  buf_free(word);
  buf_free(pattern);
  return found ? match.start : ctx.pos;
}

static size_t word_under_cursor_next(struct motion_context ctx) {
  return word_under_cursor(ctx, ctx.pos, SEARCH_FORWARDS);
}

static size_t word_under_cursor_prev(struct motion_context ctx) {
  size_t start = is_word_start(ctx) ? ctx.pos : prev_word_start(ctx);
  return word_under_cursor(ctx, start, SEARCH_BACKWARDS);
}

static size_t matching_paren(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  char a = '\0';
  size_t start;
  for (start = ctx.pos; !is_line_end(gb, start); ++start) {
    a = gb_getchar(gb, start);
    if (strchr("()[]{}", a)) {
      break;
    }
  }
  if (!strchr("()[]{}", a)) {
    return ctx.pos;
  }

  char b = *(strchr("()([][{}{", a) + 1);
  bool forwards = strchr("([{", a) != NULL;

  int nested = -1;
  size_t end;
  for (end = start; end < gb_size(gb) - 1; forwards ? end++ : end--) {
    char c = gb_getchar(gb, end);
    if (c == a) {
      nested++;
    } else if (c == b) {
      if (!nested) {
        break;
      }
      nested--;
    }
  }

  return gb_getchar(gb, end) == b ? end : ctx.pos;
}

#define LINEWISE true, false
#define EXCLUSIVE false, true
#define INCLUSIVE false, false
#define REPEAT true
#define DIRECT false

static struct motion motion_table[] = {
  {'h', left, EXCLUSIVE, REPEAT},
  {'j', down, LINEWISE, DIRECT},
  {'k', up, LINEWISE, DIRECT},
  {'l', right, EXCLUSIVE, REPEAT},
  {'0', line_start, EXCLUSIVE, REPEAT},
  {'$', line_end, INCLUSIVE, REPEAT},
  {'^', first_non_blank, EXCLUSIVE, REPEAT},
  {'{', paragraph_start, EXCLUSIVE, REPEAT},
  {'}', paragraph_end, EXCLUSIVE, REPEAT},
  {'b', prev_word_start, EXCLUSIVE, REPEAT},
  {'B', prev_WORD_start, EXCLUSIVE, REPEAT},
  {'w', next_word_start, EXCLUSIVE, REPEAT},
  {'W', next_WORD_start, EXCLUSIVE, REPEAT},
  {'e', next_word_end, INCLUSIVE, REPEAT},
  {'E', next_WORD_end, INCLUSIVE, REPEAT},
  {'G', goto_line, LINEWISE, DIRECT},
  {'t', till_forward_exclusive, INCLUSIVE, REPEAT},
  {'f', till_forward_inclusive, INCLUSIVE, REPEAT},
  {'T', till_backward_exclusive, INCLUSIVE, REPEAT},
  {'F', till_backward_inclusive, INCLUSIVE, REPEAT},
  {'/', forward_search, EXCLUSIVE, REPEAT},
  {'?', backward_search, EXCLUSIVE, REPEAT},
  {'n', search_next, EXCLUSIVE, REPEAT},
  {'N', search_prev, EXCLUSIVE, REPEAT},
  {'*', word_under_cursor_next, EXCLUSIVE, REPEAT},
  {'#', word_under_cursor_prev, EXCLUSIVE, REPEAT},
  {'%', matching_paren, INCLUSIVE, REPEAT},
  {-1, NULL, false, false, false}
};

static struct motion g_motion_table[] = {
  {'_', last_non_blank, INCLUSIVE, REPEAT},
  {'e', prev_word_end, INCLUSIVE, REPEAT},
  {'E', prev_WORD_end, INCLUSIVE, REPEAT},
  {'g', buffer_top, LINEWISE, REPEAT},
  {-1, NULL, false, false, false}
};

static struct motion *motion_find(struct motion *table, char name) {
  for (int i = 0; table[i].name != -1; ++i) {
    if (table[i].name == name) {
      return &table[i];
    }
  }
  return NULL;
}

struct motion *motion_get(struct editor *editor, struct tb_event *ev) {
  struct motion *table = motion_table;
  if (ev->ch == 'g') {
    table = g_motion_table;
    editor_waitkey(editor, ev);
  }
  return motion_find(table, (char) ev->ch);
}

size_t motion_apply(struct motion *motion, struct editor *editor) {
  size_t cursor = window_cursor(editor->window);
  struct motion_context ctx = {cursor, editor->window, editor};

  if (!motion->repeat) {
    size_t pos = motion->op(ctx);
    editor->count = 0;
    return pos;
  }

  unsigned int n = editor->count ? editor->count : 1;
  for (unsigned int i = 0; i < n; ++i) {
    ctx.pos = motion->op(ctx);
  }

  editor->count = 0;
  return ctx.pos;
}

struct buf *motion_word_under_cursor(struct window *window) {
  struct motion_context ctx = {window_cursor(window), window, NULL};
  size_t start = is_word_start(ctx) ? ctx.pos : prev_word_start(ctx);
  size_t end = is_word_end(ctx) ? ctx.pos : next_word_end(ctx);
  return gb_getstring(window->buffer->text, start, end - start + 1);
}

struct buf *motion_word_before_cursor(struct window *window) {
  size_t cursor = window_cursor(window);
  struct motion_context ctx = {cursor, window, NULL};
  size_t start = is_word_start(ctx) ? ctx.pos : prev_word_start(ctx);
  size_t end = gb_utf8prev(window->buffer->text, cursor);
  return gb_getstring(window->buffer->text, start, end - start + 1);
}

static bool isfname(char c) {
  return isalnum(c) || strchr("_-./", c) != NULL;
}

static bool is_fname_start(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return true;
  }

  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, ctx.pos - 1);
  return isfname(this) && !isfname(last);
}

static bool is_fname_end(struct motion_context ctx) {
  struct gapbuf *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1 || is_blank_line(gb, ctx.pos)) {
    return true;
  }

  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, ctx.pos + 1);
  return isfname(this) && !isfname(next);
}

struct buf *motion_filename_under_cursor(struct window *window) {
  struct motion_context ctx = {window_cursor(window), window, NULL};
  size_t start = is_fname_start(ctx) ? ctx.pos : prev_until(ctx, is_fname_start);
  size_t end = is_fname_end(ctx) ? ctx.pos : next_until(ctx, is_fname_end);
  return gb_getstring(window->buffer->text, start, end - start + 1);
}
