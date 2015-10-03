#include "motion.h"

#include <ctype.h>
#include <stdbool.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "window.h"
#include "util.h"

static bool is_line_start(struct gapbuf_t *gb, size_t pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static bool is_line_end(struct gapbuf_t *gb, size_t pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos) == '\n';
}

static bool is_first_line(struct gapbuf_t *gb, size_t pos) {
  return pos <= gb->lines->buf[0];
}

static bool is_last_line(struct gapbuf_t *gb, size_t pos) {
  return pos >= gb_size(gb) - 1 - gb->lines->buf[gb->lines->len - 1];
}

static bool is_blank_line(struct gapbuf_t *gb, size_t pos) {
  return gb_getchar(gb, pos) == '\n' && is_line_start(gb, pos);
}

static size_t left(struct motion_context_t ctx) {
  return is_line_start(ctx.window->buffer->text, ctx.pos) ? ctx.pos : ctx.pos - 1;
}

static size_t right(struct motion_context_t ctx) {
  return is_line_end(ctx.window->buffer->text, ctx.pos) ? ctx.pos : ctx.pos + 1;
}

static size_t up(struct motion_context_t ctx) {
  if (is_first_line(ctx.window->buffer->text, ctx.pos)) {
    return ctx.pos;
  }
  struct gapbuf_t *gb = ctx.window->buffer->text;
  size_t x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  return gb_linecol_to_pos(gb, y - 1, min(x, gb->lines->buf[y - 1]));
}

static size_t down(struct motion_context_t ctx) {
  if (is_last_line(ctx.window->buffer->text, ctx.pos)) {
    return ctx.pos;
  }
  struct gapbuf_t *gb = ctx.window->buffer->text;
  size_t x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  return gb_linecol_to_pos(gb, y + 1, min(x, gb->lines->buf[y + 1]));
}

static size_t line_start(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }
  return (size_t) (gb_lastindexof(gb, '\n', ctx.pos - 1) + 1);
}

static size_t line_end(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }
  return gb_indexof(gb, '\n', ctx.pos);
}

static size_t buffer_top(struct motion_context_t __unused ctx) {
  return 0;
}

static size_t first_non_blank(struct motion_context_t ctx) {
  size_t start = line_start(ctx);
  size_t end = line_end(ctx);
  struct gapbuf_t *gb = ctx.window->buffer->text;
  while (start < end && isspace(gb_getchar(gb, start))) {
    start++;
  }
  return start;
}

static size_t last_non_blank(struct motion_context_t ctx) {
  size_t start = line_start(ctx);
  size_t end = line_end(ctx);
  struct gapbuf_t *gb = ctx.window->buffer->text;
  while (end > start && isspace(gb_getchar(gb, end))) {
    end--;
  }
  return end;
}

static bool is_word_char(char c) {
  return isalnum(c) || c == '_';
}

static bool is_word_start(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, ctx.pos - 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(last) == 1);
}

static bool is_WORD_start(struct motion_context_t ctx) {
  if (ctx.pos == 0) {
    return true;
  }
  struct gapbuf_t *gb = ctx.window->buffer->text;
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, ctx.pos - 1);
  return !isspace(this) && isspace(last);
}

static bool is_word_end(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1 || is_blank_line(gb, ctx.pos)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, ctx.pos + 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(next) == 1);
}

static bool is_WORD_end(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb)) {
    return true;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, ctx.pos + 1);
  return !isspace(this) && isspace(next);
}

static size_t prev_until(struct motion_context_t ctx, bool (*pred)(struct motion_context_t)) {
  if (ctx.pos > 0) {
    do {
      --ctx.pos;
    } while (!pred(ctx) && ctx.pos > 0);
  }
  return ctx.pos;
}

static size_t next_until(struct motion_context_t ctx, bool (*pred)(struct motion_context_t)) {
  size_t size = gb_size(ctx.window->buffer->text);
  if (ctx.pos < size - 1) {
    do {
      ++ctx.pos;
    } while (!pred(ctx) && ctx.pos < size - 1);
  }
  return ctx.pos;
}

static size_t next_word_start(struct motion_context_t ctx) {
  return next_until(ctx, is_word_start);
}

static size_t next_WORD_start(struct motion_context_t ctx) {
  return next_until(ctx, is_WORD_start);
}

static size_t prev_word_start(struct motion_context_t ctx) {
  return prev_until(ctx, is_word_start);
}

static size_t prev_WORD_start(struct motion_context_t ctx) {
  return prev_until(ctx, is_WORD_start);
}

static size_t next_word_end(struct motion_context_t ctx) {
  return next_until(ctx, is_word_end);
}

static size_t next_WORD_end(struct motion_context_t ctx) {
  return next_until(ctx, is_WORD_end);
}

static size_t prev_word_end(struct motion_context_t ctx) {
  return prev_until(ctx, is_word_end);
}

static size_t prev_WORD_end(struct motion_context_t ctx) {
  return prev_until(ctx, is_WORD_end);
}

static bool is_paragraph_start(struct motion_context_t ctx) {
  if (ctx.pos == 0) {
    return true;
  }
  struct gapbuf_t *gb = ctx.window->buffer->text;
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos + 1);
}

static bool is_paragraph_end(struct motion_context_t ctx) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1) {
    return true;
  }
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos - 1);
}

static size_t paragraph_start(struct motion_context_t ctx) {
  return prev_until(ctx, is_paragraph_start);
}

static size_t paragraph_end(struct motion_context_t ctx) {
  return next_until(ctx, is_paragraph_end);
}

static size_t till_forward(struct motion_context_t ctx, bool inclusive) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
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

static size_t till_backward(struct motion_context_t ctx, bool inclusive) {
  struct gapbuf_t *gb = ctx.window->buffer->text;
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

static size_t till_forward_inclusive(struct motion_context_t ctx) {
  return till_forward(ctx, true);
}

static size_t till_forward_exclusive(struct motion_context_t ctx) {
  return till_forward(ctx, false);
}

static size_t till_backward_inclusive(struct motion_context_t ctx) {
  return till_backward(ctx, true);
}

static size_t till_backward_exclusive(struct motion_context_t ctx) {
  return till_backward(ctx, false);
}

#define LINEWISE true, false
#define EXCLUSIVE false, true
#define INCLUSIVE false, false

static struct motion_t motion_table[] = {
  {'h', left, EXCLUSIVE},
  {'j', down, LINEWISE},
  {'k', up, LINEWISE},
  {'l', right, EXCLUSIVE},
  {'0', line_start, EXCLUSIVE},
  {'$', line_end, INCLUSIVE},
  {'^', first_non_blank, EXCLUSIVE},
  {'{', paragraph_start, EXCLUSIVE},
  {'}', paragraph_end, EXCLUSIVE},
  {'b', prev_word_start, EXCLUSIVE},
  {'B', prev_WORD_start, EXCLUSIVE},
  {'w', next_word_start, EXCLUSIVE},
  {'W', next_WORD_start, EXCLUSIVE},
  {'e', next_word_end, INCLUSIVE},
  {'E', next_WORD_end, INCLUSIVE},
  {'G', down, LINEWISE}, // See motion_apply...
  {'t', till_forward_exclusive, INCLUSIVE},
  {'f', till_forward_inclusive, INCLUSIVE},
  {'T', till_backward_exclusive, INCLUSIVE},
  {'F', till_backward_inclusive, INCLUSIVE},
  {-1, NULL, false, false}
};

static struct motion_t g_motion_table[] = {
  {'_', last_non_blank, INCLUSIVE},
  {'e', prev_word_end, INCLUSIVE},
  {'E', prev_WORD_end, INCLUSIVE},
  {'g', buffer_top, LINEWISE},
  {-1, NULL, false, false}
};

static struct motion_t *motion_find(struct motion_t *table, char name) {
  for (int i = 0; table[i].name != -1; ++i) {
    if (table[i].name == name) {
      return &table[i];
    }
  }
  return NULL;
}

struct motion_t *motion_get(struct editor_t *editor, struct tb_event *ev) {
  struct motion_t *table = motion_table;
  if (ev->ch == 'g') {
    table = g_motion_table;
    editor_waitkey(editor, ev);
  }
  return motion_find(table, (char) ev->ch);
}

size_t motion_apply(struct motion_t *motion, struct editor_t *editor) {
  unsigned int n = editor->count ? editor->count : 1;

  size_t cursor = window_cursor(editor->window);
  struct motion_context_t ctx = {cursor, editor->window, editor};

  size_t nlines = gb_nlines(editor->window->buffer->text);
  // TODO(isbadawi): This is a hack to make G work correctly.
  if (motion->name == 'G') {
    n = (editor->count ? editor->count : (unsigned int) nlines) - 1;
    ctx.pos = buffer_top(ctx);
  }

  for (unsigned int i = 0; i < n; ++i) {
    ctx.pos = motion->op(ctx);
  }

  editor->count = 0;
  return ctx.pos;
}

size_t motion_word_under_cursor(struct window_t *window, char *buf) {
  struct motion_context_t ctx = {window_cursor(window), window, NULL};
  size_t start = is_word_start(ctx) ? ctx.pos : prev_word_start(ctx);
  size_t end = is_word_end(ctx) ? ctx.pos : next_word_end(ctx);
  gb_getstring(window->buffer->text, start, end - start + 1, buf);
  return start;
}
