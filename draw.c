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
#include "mode.h"
#include "search.h"
#include "util.h"

#define COLOR_DEFAULT TB_DEFAULT
#define COLOR_BLACK 16
#define COLOR_RED (TB_RED - 1)
#define COLOR_YELLOW (TB_YELLOW - 1)
#define COLOR_BLUE (TB_BLUE - 1)
#define COLOR_WHITE (TB_WHITE - 1)
#define COLOR_GREY 242

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

static struct tb_cell *window_cell(struct window *window, size_t x, size_t y) {
  struct tb_cell *cells = tb_cell_buffer();
  size_t offset = (window_y(window) + y) * (size_t) tb_width() +
    window_x(window) + x;
  return &cells[offset];
}

static struct tb_cell *window_cell_for_pos(struct window *window, size_t pos) {
  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, pos, &line, &col);
  col += window_numberwidth(window);

  if (window->top <= line && line < window->top + window_h(window) &&
      window->left <= col && col < window->left + window_w(window)) {
    return window_cell(window, col - window->left, line - window->top);
  }

  return NULL;
}

static void window_change_cell(struct window *window, size_t x, size_t y, char c,
                               int fg, int bg) {
  struct tb_cell *cell = window_cell(window, x, y);
  cell->ch = (uint32_t) c;
  cell->fg = (uint16_t) fg;
  cell->bg = (uint16_t) bg;
}

static void window_draw_cursor(struct window *window) {
  struct tb_cell *cell = window_cell_for_pos(window, window_cursor(window));
  assert(cell);
  cell->fg = COLOR_BLACK;
  cell->bg = COLOR_WHITE;
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
    for (size_t pos = start + match->region.start; pos < start + match->region.end; ++pos) {
      struct tb_cell *cell = window_cell_for_pos(window, pos);
      if (cell) {
        cell->fg = COLOR_BLACK;
        cell->bg = COLOR_YELLOW;
      }
    }
  }

  search_result_free_matches(&result);
}

void window_get_ruler(struct window *window, char *buf, size_t buflen) {
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

static void window_draw_plate(struct window *window) {
  size_t w = window_w(window);
  if (window_right(window)) {
    --w;
  }

  char plate[256];
  snprintf(plate, sizeof(plate), "%s %s%s",
      *window->buffer->name ? window->buffer->name : "[No Name]",
      window->buffer->opt.modified ? "[+]" : "",
      window->buffer->opt.readonly ? "[RO]" : "");

  size_t platelen = strlen(plate);
  for (size_t x = 0; x < window_w(window); ++x) {
    char c = x < platelen ? plate[x] : ' ';
    window_change_cell(window, x, window_h(window) - 1, c, COLOR_BLACK, COLOR_WHITE);
  }
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
  size_t rulerlen = strlen(ruler);
  for (size_t i = 0; i < rulerlen; ++i) {
    window_change_cell(window, w - (rulerlen - i + 1), h, ruler[i], fg, bg);
  }
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

  size_t col = window_numberwidth(window) - 2;
  do {
    size_t digit = linenumber % 10;
    window_change_cell(window, col--, line - window->top,
        (char) digit + '0', COLOR_YELLOW, COLOR_DEFAULT);
    linenumber = (linenumber - digit) / 10;
  } while (linenumber > 0);
}

static void window_draw_visual_mode_selection(struct window *window) {
  if (!window->visual_mode_selection) {
    return;
  }

  for (size_t pos = window->visual_mode_selection->start;
      pos < window->visual_mode_selection->end; ++pos) {
    struct tb_cell *cell = window_cell_for_pos(window, pos);
    if (cell) {
      cell->fg = COLOR_WHITE;
      cell->bg = COLOR_GREY;
    }
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
    window_cell(window, x, line - window->top)->fg |= TB_UNDERLINE;
  }
}

static void window_draw_leaf(struct window *window) {
  assert(window->split_type == WINDOW_LEAF);

  struct gapbuf *gb = window->buffer->text;

  size_t w = window_w(window) - window_numberwidth(window);
  size_t h = window_h(window);

  size_t rows = min(gb->lines->len - window->top, h);
  size_t numberwidth = window_numberwidth(window);

  for (size_t y = 0; y < rows; ++y) {
    size_t line = y + window->top;
    window_draw_line_number(window, line);

    size_t cols = (size_t) max(0,
        min((ssize_t) gb->lines->buf[line] - (ssize_t) window->left, (ssize_t) w));
    for (size_t x = 0; x < cols; ++x) {
      size_t col = x + window->left;
      size_t pos = gb_linecol_to_pos(gb, line, col);
      char c = gb_getchar(gb, pos);
      window_change_cell(window, numberwidth + x, y, c, COLOR_WHITE, COLOR_DEFAULT);
    }
  }
  window_draw_visual_mode_selection(window);
  window_draw_cursorline(window);

  size_t nlines = gb_nlines(window->buffer->text);
  for (size_t y = nlines - window->top; y < h; ++y) {
    window_change_cell(window, 0, y, '~', COLOR_BLUE, COLOR_DEFAULT);
  }

  if (window_should_draw_plate(window)) {
    window_draw_plate(window);
  }
}

static void window_draw(struct window *window) {
  if (window->split_type == WINDOW_LEAF) {
    window_draw_leaf(window);
    return;
  }

  window_draw(window->split.first);
  window_draw(window->split.second);
  if (window->split_type == WINDOW_SPLIT_VERTICAL) {
    struct window *left = window->split.first;
    for (size_t y = 0; y < window_h(left) - 1; ++y) {
      window_change_cell(left, window_w(left) - 1, y, '|', COLOR_BLACK, COLOR_WHITE);
    }
  }
}

void editor_draw(struct editor *editor) {
  tb_clear();

  // FIXME(ibadawi): It's a bit kludgy to call this here.
  // We want to visual mode selection to be up to date if we're searching inside
  // visual mode with 'incsearch' enabled.
  visual_mode_selection_update(editor);

  window_scroll(editor->window, (size_t) editor->opt.sidescroll);

  window_draw(window_root(editor->window));

  struct search_match *match = editor->window->incsearch_match;
  if (match) {
    for (size_t pos = match->region.start; pos < match->region.end; ++pos) {
      struct tb_cell *cell = window_cell_for_pos(editor->window, pos);
      assert(cell);
      cell->fg = COLOR_BLACK;
      cell->bg = COLOR_WHITE;
    }
  }

  if (editor->opt.hlsearch && editor->highlight_search_matches) {
    struct editor_register *lsp = editor_get_register(editor, '/');
    char *pattern = lsp->read(lsp);
    if (*pattern) {
      bool ignore_case = editor_ignore_case(editor, pattern);
      window_draw_search_matches(
          window_root(editor->window), pattern, ignore_case);
    }
    free(pattern);
  }
  window_draw_cursor(editor->window);

  for (size_t x = 0; x < editor->status->len; ++x) {
    tb_change_cell((int) x, (int) editor->height - 1, (uint32_t) editor->status->buf[x],
        editor->status_error ? COLOR_DEFAULT : COLOR_WHITE,
        editor->status_error ? COLOR_RED : COLOR_DEFAULT);
  }

  if (editor->status_cursor) {
    tb_change_cell((int) editor->status_cursor, (int) editor->height - 1,
        (uint32_t) editor->status->buf[editor->status_cursor],
        COLOR_BLACK, COLOR_WHITE);
  }

  if (editor->opt.ruler &&
      (editor->window->parent || !editor->status_cursor)) {
    window_draw_ruler(window_root(editor->window));
  }

  tb_present();
}
