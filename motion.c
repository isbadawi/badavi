#include "motion.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <termbox.h>

#include "editor.h"
#include "mode.h"
#include "util.h"

static bool is_line_start(gapbuf_t *gb, int pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static bool is_line_end(gapbuf_t *gb, int pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos) == '\n';
}

static bool is_first_line(gapbuf_t *gb, int pos) {
  return pos < gb->lines->buf[0];
}

static bool is_last_line(gapbuf_t *gb, int pos) {
  return pos >= gb_size(gb) - 1 - gb->lines->buf[gb->lines->len - 1];
}

static bool is_blank_line(gapbuf_t *gb, int pos) {
  return gb_getchar(gb, pos) == '\n' && is_line_start(gb, pos);
}

static int left(motion_context_t ctx) {
  return is_line_start(ctx.window->buffer->text, ctx.pos) ? ctx.pos : ctx.pos - 1;
}

static int right(motion_context_t ctx) {
  return is_line_end(ctx.window->buffer->text, ctx.pos) ? ctx.pos : ctx.pos + 1;
}

static int up(motion_context_t ctx) {
  if (is_first_line(ctx.window->buffer->text, ctx.pos)) {
    return ctx.pos;
  }
  gapbuf_t *gb = ctx.window->buffer->text;
  int x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  return gb_linecol_to_pos(gb, y - 1, min(x, gb->lines->buf[y - 1]));
}

static int down(motion_context_t ctx) {
  if (is_last_line(ctx.window->buffer->text, ctx.pos)) {
    return ctx.pos;
  }
  gapbuf_t *gb = ctx.window->buffer->text;
  int x, y;
  gb_pos_to_linecol(gb, ctx.pos, &y, &x);
  return gb_linecol_to_pos(gb, y + 1, min(x, gb->lines->buf[y + 1]));
}

static int line_start(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }
  return gb_lastindexof(gb, '\n', ctx.pos - 1) + 1;
}

static int line_end(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }
  return gb_indexof(gb, '\n', ctx.pos);
}

static int buffer_top(motion_context_t __unused ctx) {
  return 0;
}

static int first_non_blank(motion_context_t ctx) {
  int start = line_start(ctx);
  int end = line_end(ctx);
  gapbuf_t *gb = ctx.window->buffer->text;
  while (start < end && isspace(gb_getchar(gb, start))) {
    start++;
  }
  return start;
}

static int last_non_blank(motion_context_t ctx) {
  int start = line_start(ctx);
  int end = line_end(ctx);
  gapbuf_t *gb = ctx.window->buffer->text;
  while (end > start && isspace(gb_getchar(gb, end))) {
    end--;
  }
  return end;
}

static bool is_word_char(char c) {
  return isalnum(c) || c == '_';
}

static bool is_word_start(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return 1;
  }
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, ctx.pos - 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(last) == 1);
}

static bool is_WORD_start(motion_context_t ctx) {
  if (ctx.pos == 0) {
    return 1;
  }
  gapbuf_t *gb = ctx.window->buffer->text;
  char this = gb_getchar(gb, ctx.pos);
  char last = gb_getchar(gb, ctx.pos - 1);
  return !isspace(this) && isspace(last);
}

static bool is_word_end(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1 || is_blank_line(gb, ctx.pos)) {
    return 1;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, ctx.pos + 1);
  return !isspace(this) && (is_word_char(this) + is_word_char(next) == 1);
}

static bool is_WORD_end(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb)) {
    return 1;
  }
  char this = gb_getchar(gb, ctx.pos);
  char next = gb_getchar(gb, ctx.pos + 1);
  return !isspace(this) && isspace(next);
}

static int prev_until(motion_context_t ctx, bool (*pred)(motion_context_t)) {
  if (ctx.pos > 0) {
    do {
      --ctx.pos;
    } while (!pred(ctx) && ctx.pos > 0);
  }
  return ctx.pos;
}

static int next_until(motion_context_t ctx, bool (*pred)(motion_context_t)) {
  int size = gb_size(ctx.window->buffer->text);
  if (ctx.pos < size - 1) {
    do {
      ++ctx.pos;
    } while (!pred(ctx) && ctx.pos < size - 1);
  }
  return ctx.pos;
}

static int next_word_start(motion_context_t ctx) {
  return next_until(ctx, is_word_start);
}

static int next_WORD_start(motion_context_t ctx) {
  return next_until(ctx, is_WORD_start);
}

static int prev_word_start(motion_context_t ctx) {
  return prev_until(ctx, is_word_start);
}

static int prev_WORD_start(motion_context_t ctx) {
  return prev_until(ctx, is_WORD_start);
}

static int next_word_end(motion_context_t ctx) {
  return next_until(ctx, is_word_end);
}

static int next_WORD_end(motion_context_t ctx) {
  return next_until(ctx, is_WORD_end);
}

static int prev_word_end(motion_context_t ctx) {
  return prev_until(ctx, is_word_end);
}

static int prev_WORD_end(motion_context_t ctx) {
  return prev_until(ctx, is_WORD_end);
}

static bool is_paragraph_start(motion_context_t ctx) {
  if (ctx.pos == 0) {
    return 1;
  }
  gapbuf_t *gb = ctx.window->buffer->text;
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos + 1);
}

static bool is_paragraph_end(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (ctx.pos == gb_size(gb) - 1) {
    return 1;
  }
  return is_blank_line(gb, ctx.pos) && !is_blank_line(gb, ctx.pos - 1);
}

static int paragraph_start(motion_context_t ctx) {
  return prev_until(ctx, is_paragraph_start);
}

static int paragraph_end(motion_context_t ctx) {
  return next_until(ctx, is_paragraph_end);
}

#define LINEWISE true, false
#define EXCLUSIVE false, true
#define INCLUSIVE false, false

static motion_t motion_table[] = {
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
  {-1, NULL, false, false}
};

static motion_t g_motion_table[] = {
  {'_', last_non_blank, INCLUSIVE},
  {'e', prev_word_end, INCLUSIVE},
  {'E', prev_WORD_end, INCLUSIVE},
  {'g', buffer_top, LINEWISE},
  {-1, NULL, false, false}
};

static motion_t *motion_find(motion_t *table, char name) {
  for (int i = 0; table[i].name != -1; ++i) {
    if (table[i].name == name) {
      return &table[i];
    }
  }
  return NULL;
}

static void no_op(editor_t __unused *editor) {
}

static void g_pressed(editor_t *editor, struct tb_event *ev) {
  motion_t *motion = motion_find(g_motion_table, ev->ch);
  if (motion) {
    editor->motion = motion;
  }
  editor_pop_mode(editor);
  editor_pop_mode(editor);
}

static editing_mode_t g_mode = { no_op, g_pressed };

typedef struct {
  editing_mode_t mode;
  char which; // t or T or f or F
} till_mode_t;

typedef struct {
  motion_t motion;
  char arg;
} till_motion_t;

static till_motion_t till_motion_impl = {{-1, NULL, INCLUSIVE}, -1};

static int till_forward_inclusive(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = ((till_motion_t*) ctx.motion)->arg;
  int newline = gb_indexof(gb, '\n', ctx.pos + 1);
  int target = gb_indexof(gb, arg, ctx.pos + 1);
  if (target < newline) {
    return target;
  }
  return ctx.pos;
}

static int till_forward_exclusive(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_end(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = ((till_motion_t*) ctx.motion)->arg;
  int newline = gb_indexof(gb, '\n', ctx.pos + 1);
  int target = gb_indexof(gb, arg, ctx.pos + 1);
  if (target < newline) {
    return target - 1;
  }
  return ctx.pos;
}

static int till_backward_inclusive(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = ((till_motion_t*) ctx.motion)->arg;
  int newline = gb_lastindexof(gb, '\n', ctx.pos - 1);
  int target = gb_lastindexof(gb, arg, ctx.pos - 1);
  if (target > newline) {
    return target;
  }
  return ctx.pos;
}

static int till_backward_exclusive(motion_context_t ctx) {
  gapbuf_t *gb = ctx.window->buffer->text;
  if (is_line_start(gb, ctx.pos)) {
    return ctx.pos;
  }

  char arg = ((till_motion_t*) ctx.motion)->arg;
  int newline = gb_lastindexof(gb, '\n', ctx.pos - 1);
  int target = gb_lastindexof(gb, arg, ctx.pos - 1);
  if (target > newline) {
    return target + 1;
  }
  return ctx.pos;
}

static void till_key_pressed(editor_t *editor, struct tb_event *ev) {
  till_mode_t *till = (till_mode_t*) editor->mode;
  till_motion_impl.motion.name = till->which;
  switch (till->which) {
    case 't': till_motion_impl.motion.op = till_forward_exclusive; break;
    case 'T': till_motion_impl.motion.op = till_backward_exclusive; break;
    case 'f': till_motion_impl.motion.op = till_forward_inclusive; break;
    case 'F': till_motion_impl.motion.op = till_backward_inclusive; break;
  }
  till_motion_impl.arg = ev->ch;
  if (ev->key == TB_KEY_SPACE) {
    till_motion_impl.arg = ' ';
  }
  editor->motion = (motion_t*) &till_motion_impl;
  editor_pop_mode(editor);
  editor_pop_mode(editor);
}

static till_mode_t till_mode_impl = {
  {no_op, till_key_pressed},
  -1
};

static editing_mode_t *till_mode(char which) {
  till_mode_impl.which = which;
  return (editing_mode_t*) &till_mode_impl;
}

static void key_pressed(editor_t *editor, struct tb_event *ev) {
  if (ev->ch == 'g') {
    editor_push_mode(editor, &g_mode);
    return;
  }
  if (strchr("tTfF", ev->ch)) {
    editor_push_mode(editor, till_mode(ev->ch));
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
  int n = editor->count ? editor->count : 1;

  motion_context_t ctx = {editor->window->cursor, editor->window, motion};

  // TODO(isbadawi): This is a hack to make G work correctly.
  if (motion->name == 'G') {
    n = editor->count ? editor->count - 1 : gb_nlines(editor->window->buffer->text) - 1;
    ctx.pos = buffer_top(ctx);
  }

  for (int i = 0; i < n; ++i) {
    ctx.pos = motion->op(ctx);
  }

  editor->count = 0;
  editor->motion = NULL;
  return ctx.pos;
}
