#include "window.h"

#include <stdbool.h>
#include <stdlib.h>

#include <termbox.h>

#include "options.h"
#include "util.h"

window_t *window_create(buffer_t *buffer, size_t x, size_t y, size_t w, size_t h) {
  window_t *window = malloc(sizeof(window_t));
  if (!window) {
    return NULL;
  }

  window->buffer = buffer;
  window->top = 0;
  window->left = 0;
  window->cursor = 0;

  window->x = x;
  window->y = y;
  window->w = w;
  window->h = h;

  return window;
}

// Number of columns to use for the line number (including the trailing space).
// TODO(isbadawi): This might not be exactly correct for relativenumber mode.
static size_t window_numberwidth(window_t* window) {
  if (!option_get_bool("number") && !option_get_bool("relativenumber")) {
    return 0;
  }

  size_t nlines = window->buffer->text->lines->len;
  char buf[10];
  size_t maxwidth = (size_t) snprintf(buf, 10, "%zu", nlines);

  return max((size_t) option_get_int("numberwidth"), maxwidth + 1);
}

static void window_ensure_cursor_visible(window_t *window) {
  size_t x, y;
  gb_pos_to_linecol(window->buffer->text, window->cursor, &y, &x);

  size_t w = window->w - window_numberwidth(window);
  size_t h = window->h;

  // TODO(isbadawi): Clean this up...
  // The problem is that x & w (and y & h) are unsigned, but we want to treat
  // their difference as signed. All these casts are necessary to get around
  // warnings.
  window->left = (size_t) ((ssize_t) max((ssize_t) min(window->left, x), (ssize_t) (x - w + 1)));
  window->top = (size_t) ((ssize_t) max((ssize_t) min(window->top, y), (ssize_t) (y - h + 1)));
}

static void window_change_cell(window_t *window, size_t x, size_t y, char c,
                               int fg, int bg) {
  tb_change_cell(
      (int) (window_numberwidth(window) + window->x + x),
      (int) (window->y + y),
      (uint32_t) c, (uint16_t) fg, (uint16_t) bg);
}

static void window_draw_cursor(window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  char c = gb_getchar(gb, window->cursor);
  size_t x, y;
  gb_pos_to_linecol(gb, window->cursor, &y, &x);

  window_change_cell(window,
      x - window->left,
      y - window->top,
      c == '\n' ? ' ' : c,
      TB_DEFAULT,
      TB_WHITE);
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  size_t cursorline, cursorcol;
  gb_pos_to_linecol(gb, window->cursor, &cursorline, &cursorcol);

  bool number = option_get_bool("number");
  bool relativenumber = option_get_bool("relativenumber");

  size_t numberwidth = window_numberwidth(window);

  size_t w = window->w - numberwidth;
  size_t h = window->h;

  size_t topy = window->top;
  size_t topx = window->left;
  size_t rows = min(gb->lines->len - topy, h);
  for (size_t y = 0; y < rows; ++y) {
    size_t absolute = window->top + y + 1;
    size_t relative = (size_t) labs((ssize_t)(absolute - cursorline - 1));
    size_t linenumber = absolute;
    if (number || relativenumber) {
      if (relativenumber && !(number && relative == 0)) {
        linenumber = relative;
      }

      size_t col = numberwidth - 2;
      do {
        size_t digit = linenumber % 10;
        tb_change_cell((int) col--, (int) y, (uint32_t) (digit + '0'),
                       TB_YELLOW, TB_DEFAULT);
        linenumber = (linenumber - digit) / 10;
      } while (linenumber > 0);
    }

    bool drawcursorline = relative == 0 && option_get_bool("cursorline");
    int fg = drawcursorline ? (TB_WHITE | TB_UNDERLINE) : TB_WHITE;

    size_t cols = (size_t) max(0, min((ssize_t) gb->lines->buf[y + topy] - (ssize_t) topx, (ssize_t) w));
    for (size_t x = 0; x < cols; ++x) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, y + topy, x + topx));
      window_change_cell(window, x, y, c, fg, TB_DEFAULT);
    }

    if (drawcursorline) {
      for (size_t x = cols; x < w; ++x) {
        window_change_cell(window, x, y, ' ', fg, TB_DEFAULT);
      }
    }
  }

  size_t nlines = gb_nlines(window->buffer->text);
  for (size_t y = nlines; y < window->y + window->h; ++y) {
    tb_change_cell((int) window->x, (int) y, '~', TB_BLUE, TB_DEFAULT);
  }

  window_draw_cursor(window);
}
