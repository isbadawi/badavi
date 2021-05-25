#include "window.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "search.h"
#include "util.h"

#define COLOR_DEFAULT TB_DEFAULT
#define COLOR_BLACK TB_BLACK
#define COLOR_RED TB_RED
#define COLOR_YELLOW TB_YELLOW
#define COLOR_BLUE TB_BLUE
#define COLOR_WHITE 0x07
#define COLOR_GREY TB_DARK_GRAY

// Number of columns to use for the line number (including the trailing space).
static size_t window_numberwidth(struct window* window) {
  size_t largest;
  if (window->opt.number) {
    largest = gb_nlines(window->buffer->text);
  } else if (window->opt.relativenumber) {
    largest = window_h(window) / 2;
  } else {
    return 0;
  }
  char buf[10];
  size_t maxwidth = (size_t) snprintf(buf, sizeof(buf), "%zu", largest);

  return max((size_t) window->opt.numberwidth, maxwidth + 1);
}

static bool window_should_draw_plate(struct window *window) {
  return window->parent != NULL;
}

static void window_scroll(struct window *window, size_t scroll) {
  if (window->split_type != WINDOW_LEAF) {
    window_scroll(window->split.first, scroll);
    window_scroll(window->split.second, scroll);
    return;
  }

  struct gapbuf *gb = window->buffer->text;
  size_t x, y;
  gb_pos_to_linecol(gb, window_cursor(window), &y, &x);

  size_t w = window_w(window) - window_numberwidth(window);
  size_t h = window_h(window);
  if (window_right(window)) {
    --w;
  }
  if (window_should_draw_plate(window)) {
    --h;
  }

  if (y < window->top) {
    window->top = y;
  } else if (y > window->top + h - 1) {
    window->top = y - h + 1;
  }

  bool left = x < window->left;
  bool right = x > window->left + w - 1;
  if (left || right) {
    if (!scroll) {
      window->left = x >= w / 2 ? x - w / 2 : 0;
    } else if (left) {
      if (window->left - x >= scroll) {
        window->left = x;
      } else {
        window->left = window->left >= scroll ? window->left - scroll : 0;
      }
    } else if (right) {
      if (x - (window->left + w - 1) >= scroll) {
        window->left = x - w + 1;
      } else {
        window->left = min(window->left + scroll, gb->lines->buf[y] - 1);
      }
    }
  }
}

static bool window_pos_to_xy(
    struct window *window, size_t pos, size_t *x, size_t *y) {
  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, pos, &line, &col);
  int tabstop = window->buffer->opt.tabstop;
  size_t tabs = 0;
  for (size_t i = 0; i < col; ++i) {
    if (gb_getchar(window->buffer->text, pos - (i + 1)) == '\t') {
      ++tabs;
    }
  }
  col += window_numberwidth(window) + tabs * (tabstop - 1);
  if (gb_getchar(window->buffer->text, pos) == '\t') {
    col += tabstop - 1;
  }

  if (window->top <= line && line < window->top + window_h(window) &&
      window->left <= col && col < window->left + window_w(window)) {
    *x = col - window->left;
    *y = line - window->top;
    return true;
  }
  return false;
}

#define W2SX(x) (window_x(window) + x)
#define W2SY(y) (window_y(window) + y)
#define W2S(x, y) W2SX(x), W2SY(y)
#define CELL(x, y) (tb_cell_buffer() + (W2SY(y) * (size_t) tb_width() + W2SX(x)))
#define CELLFGBG(pos, f, b) { \
  size_t x, y; \
  if (window_pos_to_xy(window, pos, &x, &y)) { \
    struct tb_cell *cell = CELL(x, y); \
    cell->fg = f; \
    cell->bg = b; \
  } \
}
#define REGIONFGBG(region, offset, fg, bg) { \
  for (size_t pos = offset + (region)->start; pos < offset + (region)->end; ++pos) { \
    CELLFGBG(pos, fg, bg); \
  } \
}

// FIXME(ibadawi): avoid re-searching if e.g. we just moved the cursor
static void window_draw_search_matches(struct window *window,
                                       char *pattern, bool ignore_case) {
  if (window->split_type != WINDOW_LEAF) {
    window_draw_search_matches(window->split.first, pattern, ignore_case);
    window_draw_search_matches(window->split.second, pattern, ignore_case);
    return;
  }

  struct gapbuf *gb = window->buffer->text;
  size_t h = window_h(window);
  if (window_should_draw_plate(window)) {
    --h;
  }

  size_t top = window->top;
  size_t bot = window->top + min(gb->lines->len - window->top, h) - 1;

  size_t start = gb_linecol_to_pos(gb, top, 0);
  size_t end = gb_linecol_to_pos(gb, bot, gb->lines->buf[bot]);

  struct buf *text = gb_getstring(gb, start, end - start);

  struct search_result result;
  regex_search(text->buf, pattern, ignore_case, &result);
  buf_free(text);
  if (!result.ok) {
    return;
  }

  struct search_match *match;
  TAILQ_FOREACH(match, &result.matches, pointers) {
    REGIONFGBG(&match->region, start, COLOR_BLACK, COLOR_YELLOW);
  }

  search_result_free_matches(&result);
}

static void window_get_ruler(struct window *window, char *buf, size_t buflen) {
  struct gapbuf *gb = window->buffer->text;
  size_t line, col;
  gb_pos_to_linecol(gb, window_cursor(window), &line, &col);
  size_t nlines = gb_nlines(gb);
  size_t h = window_h(window);

  bool top = window->top == 0;
  bool bot = nlines < window->top + h + 1;
  char relpos[4];
  if (top && bot) {
    strcpy(relpos, "All");
  } else if (top) {
    strcpy(relpos, "Top");
  } else if (bot) {
    strcpy(relpos, "Bot");
  } else {
    size_t above = window->top - 1;
    size_t below = nlines - (window->top + h) + 1;
    size_t pct = (above * 100 / (above + below));
    snprintf(relpos, sizeof(relpos), "%zu%%", pct);
  }

  snprintf(buf, buflen, "%zu,%zu        %3s", line + 1, col + 1, relpos);
}

static void window_draw_plate(struct window *window, struct editor *editor) {
  size_t w = window_w(window);
  if (window_right(window)) {
    --w;
  }

  const char *path = editor_buffer_name(editor, window->buffer);

  char plate[256];
  snprintf(plate, sizeof(plate), "%s %s%s", path,
      window->buffer->opt.modified ? "[+]" : "",
      window->buffer->opt.readonly ? "[RO]" : "");

  size_t x = W2SX(0);
  size_t y = W2SY(window_h(window) - 1);
  tb_empty(x, y, COLOR_WHITE, window_w(window));
  tb_string(x, y, COLOR_BLACK, COLOR_WHITE, plate);
}

static void window_draw_ruler(struct window *window) {
  if (window->split_type != WINDOW_LEAF) {
    window_draw_ruler(window->split.first);
    window_draw_ruler(window->split.second);
    return;
  }

  size_t w = window_w(window);
  size_t h = window_h(window) - 1;
  int fg = COLOR_BLACK;
  int bg = COLOR_WHITE;
  if (!window->parent) {
    ++h;
    fg = COLOR_WHITE;
    bg = COLOR_DEFAULT;
  }

  char ruler[32];
  window_get_ruler(window, ruler, sizeof(ruler));
  tb_string(W2S(w - strlen(ruler) - 1, h), fg, bg, ruler);
}

static void window_draw_line_number(struct window *window, size_t line) {
  bool number = window->opt.number;
  bool relativenumber = window->opt.relativenumber;

  if (!number && !relativenumber) {
    return;
  }

  size_t cursorline, cursorcol;
  gb_pos_to_linecol(window->buffer->text, window_cursor(window),
      &cursorline, &cursorcol);

  size_t linenumber = line + 1;
  if (relativenumber && !(number && line == cursorline)) {
    linenumber = (size_t) labs((ssize_t)(line - cursorline));
  }
  bool left = number && relativenumber && line == cursorline;

  tb_stringf(W2S(0, line - window->top), COLOR_YELLOW, COLOR_DEFAULT,
      left ? "%-*zu": "%*zu", window_numberwidth(window) - 1, linenumber);
}

static void window_draw_visual_mode_selection(struct window *window) {
  if (window->visual_mode_selection) {
    REGIONFGBG(window->visual_mode_selection, 0, COLOR_WHITE, COLOR_GREY);
  }
}

static void window_draw_cursorline(struct window *window) {
  if (!window->opt.cursorline || window->visual_mode_selection) {
    return;
  }

  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, window_cursor(window), &line, &col);

  size_t numberwidth = window_numberwidth(window);
  size_t width = window_w(window);
  for (size_t x = numberwidth; x < width; ++x) {
    CELL(x, line - window->top)->fg |= TB_UNDERLINE;
  }
}

// TODO(ibadawi): support different colorschemes.
// These hardcoded colors are from wombat256.
static tb_color token_color(enum syntax_token_kind token) {
  switch (token) {
  case SYNTAX_TOKEN_COMMENT: return 246;
  case SYNTAX_TOKEN_IDENTIFIER: return TB_DEFAULT;
  case SYNTAX_TOKEN_LITERAL_CHAR: return 194;
  case SYNTAX_TOKEN_LITERAL_NUMBER: return 173;
  case SYNTAX_TOKEN_LITERAL_STRING: return 113;
  case SYNTAX_TOKEN_PREPROC: return 173;
  case SYNTAX_TOKEN_PUNCTUATION: return TB_DEFAULT;
  case SYNTAX_TOKEN_STATEMENT: return 111;
  case SYNTAX_TOKEN_TYPE: return 192;
  default: assert(0); return -1;
  }
}

static void window_draw_leaf(struct window *window, struct editor *editor) {
  assert(window->split_type == WINDOW_LEAF);

  struct gapbuf *gb = window->buffer->text;

  size_t w = window_w(window) - window_numberwidth(window);
  size_t h = window_h(window);

  size_t rows = min(gb->lines->len - window->top, h);
  size_t numberwidth = window_numberwidth(window);
  int tabstop = window->buffer->opt.tabstop;

  struct syntax syntax;
  struct syntax_token token = {SYNTAX_TOKEN_NONE, 0, 0};
  bool highlight = syntax_init(&syntax, window->buffer);

  for (size_t y = 0; y < rows; ++y) {
    size_t line = y + window->top;
    window_draw_line_number(window, line);

    size_t cols = (size_t) max(0,
        min((ssize_t) gb_utf8len_line(gb, line) - (ssize_t) window->left, (ssize_t) w));
    size_t tabs = 0;
    for (size_t x = 0; x < cols; ++x) {
      size_t x_offset = tabs * (tabstop - 1) + numberwidth + x;
      size_t col = x + window->left;
      size_t pos = gb_linecol_to_pos(gb, line, col);

      tb_color fg = COLOR_WHITE;
      tb_color bg = COLOR_DEFAULT;

      if (highlight) {
        if (token.kind == SYNTAX_TOKEN_NONE ||
            !(token.pos <= pos && pos < token.pos + token.len)) {
          syntax_token_at(&syntax, &token, pos);
        }
        fg = token_color(token.kind);
      }

      char c = gb_getchar(gb, pos);
      if (c == '\r') {
        continue;
      }
      if (c == '\t') {
        ++tabs;
        tb_stringf(W2S(x_offset, y), fg, bg, "%*s", tabstop, "");
      } else {
        tb_char(W2S(x_offset, y), fg, bg, gb_utf8(gb, pos));
      }
    }
  }
  window_draw_visual_mode_selection(window);
  window_draw_cursorline(window);

  size_t nlines = gb_nlines(window->buffer->text);
  for (size_t y = nlines - window->top; y < h; ++y) {
    tb_char(W2S(0, y), COLOR_BLUE, COLOR_DEFAULT, '~');
  }

  if (window_should_draw_plate(window)) {
    window_draw_plate(window, editor);
  }
}

static void window_draw(struct window *window, struct editor *editor) {
  if (window->split_type == WINDOW_LEAF) {
    window_draw_leaf(window, editor);
    return;
  }

  window_draw(window->split.first, editor);
  window_draw(window->split.second, editor);
  if (window->split_type == WINDOW_SPLIT_VERTICAL) {
    struct window *left = window->split.first;
    for (size_t y = 0; y < window_h(left) - 1; ++y) {
      tb_char(W2S(window_w(left) - 1, y), COLOR_BLACK, COLOR_WHITE, '|');
    }
  }
}

static void editor_draw_window(struct editor *editor) {
  // FIXME(ibadawi): It's a bit kludgy to call this here.
  // We want to visual mode selection to be up to date if we're searching inside
  // visual mode with 'incsearch' enabled.
  visual_mode_selection_update(editor);

  struct window *window = editor->window;
  struct window *root = window_root(window);

  window_scroll(window, (size_t) editor->opt.sidescroll);
  window_draw(root, editor);

  struct search_match *match = editor->window->incsearch_match;
  if (match) {
    REGIONFGBG(&match->region, 0, COLOR_BLACK, COLOR_WHITE);
  }

  if (editor->opt.hlsearch && editor->highlight_search_matches) {
    struct editor_register *lsp = editor_get_register(editor, '/');
    char *pattern = lsp->read(lsp);
    if (*pattern) {
      bool ignore_case = editor_ignore_case(editor, pattern);
      window_draw_search_matches(root, pattern, ignore_case);
    }
    free(pattern);
  }
  CELLFGBG(window_cursor(window), COLOR_BLACK, COLOR_WHITE);

  if (editor->opt.ruler &&
      (editor->window->parent || !editor->status_cursor)) {
    window_draw_ruler(root);
  }
}

static void editor_draw_popup(struct editor *editor) {
  struct editor_popup *popup = &editor->popup;
  if (!popup->visible) {
    return;
  }

  struct window *window = editor->window;
  size_t x, y;
  bool ok = window_pos_to_xy(window, popup->pos, &x, &y);
  x--; y++;
  assert(ok);

  size_t sx = W2SX(x);
  size_t sy = W2SY(y);

  int height = min(popup->len, (int) (tb_height() - 1 - sy));

  int top = popup->offset;
  int bot = popup->offset + height;
  if (popup->selected < top + 4) {
    popup->offset = max(0, popup->selected - 3);
  } else if (popup->selected >= bot - 4) {
    popup->offset = min(popup->len, popup->selected + 4) - height;
  }

  int offset = popup->offset;
  for (int i = 0; i < height; ++i) {
    tb_color fg = i + offset == popup->selected ? TB_DARK_GREY : TB_BLACK;
    tb_color bg = i + offset == popup->selected ? TB_BLACK : TB_LIGHT_MAGENTA;
    tb_empty(sx, sy + i, bg, 50);
    tb_string_with_limit(sx + 1, sy + i, fg, bg, popup->lines[i + offset], 50);
  }

  if (height < popup->len) {
    int scrollbar_height = (int)((1.0 * height / popup->len) * height);
    int scrollbar_y = (int)((1.0 * offset / popup->len) * height);
    if (offset == popup->len - height) {
      scrollbar_y = height - scrollbar_height;
    }
    for (int i = 0; i < height; ++i) {
      tb_color color = 248;
      if (i >= scrollbar_y && i < scrollbar_y + scrollbar_height) {
        color = TB_WHITE;
      }
      tb_empty(sx + 49, sy + i, color, 1);
    }
  }

}

static void editor_draw_message(struct editor *editor) {
  if (!editor->message->len) {
    return;
  }

  int msglines = 1 + strcnt(editor->message->buf, '\n');
  memmove(
      tb_cell_buffer(),
      tb_cell_buffer() + (msglines * tb_width()),
      sizeof(struct tb_cell) * (tb_height() - msglines) * tb_width());

  int msgstart = (int) editor->height - 1 - msglines;
  for (int row = msgstart; row < msgstart + msglines; ++row) {
    tb_empty(0, row, COLOR_DEFAULT, tb_width());
  }

  int col = 0;
  for (size_t x = 0; x < editor->message->len; ++x) {
    if (editor->message->buf[x] == '\n') {
      msgstart++;
      col = 0;
      continue;
    }

    tb_char(col++, msgstart,
        COLOR_WHITE, COLOR_DEFAULT,
        (uint32_t) editor->message->buf[x]);
  }
}

static void editor_draw_status(struct editor *editor) {
  tb_string(0, (int) editor->height - 1,
      editor->status_error ? COLOR_DEFAULT : COLOR_WHITE,
      editor->status_error ? COLOR_RED : COLOR_DEFAULT,
      editor->status->buf);

  if (editor->status_cursor) {
    tb_char((int) editor->status_cursor, (int) editor->height - 1,
        COLOR_BLACK, COLOR_WHITE,
        (uint32_t) editor->status->buf[editor->status_cursor]);
  }
}

void editor_draw(struct editor *editor) {
  tb_clear_buffer();
  editor_draw_window(editor);
  editor_draw_popup(editor);
  editor_draw_message(editor);
  editor_draw_status(editor);
  tb_render();
}
