#include "window.h"

#include <stdbool.h>
#include <stdlib.h>

#include <termbox.h>

#include "options.h"
#include "util.h"

window_t *window_create(buffer_t *buffer, int x, int y, int w, int h) {
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
static int window_numberwidth(window_t* window) {
  if (!option_get_bool("number") && !option_get_bool("relativenumber")) {
    return 0;
  }

  int nlines = window->buffer->text->lines->len;
  char buf[10];
  int maxwidth = snprintf(buf, 10, "%d", nlines);

  return max(option_get_int("numberwidth"), maxwidth + 1);
}

static void window_ensure_cursor_visible(window_t *window) {
  int x, y;
  gb_pos_to_linecol(window->buffer->text, window->cursor, &y, &x);

  int w = window->w - window_numberwidth(window);
  int h = window->h;

  window->left = max(min(window->left, x), x - w + 1);
  window->top = max(min(window->top, y), y - h + 1);
}

static void window_change_cell(window_t *window, int x, int y, char c, int fg, int bg) {
  tb_change_cell(
      window_numberwidth(window) + window->x + x,
      window->y + y,
      c, fg, bg);
}

static void window_draw_cursor(window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  char c = gb_getchar(gb, window->cursor);
  int x, y;
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

  int cursorline, cursorcol;
  gb_pos_to_linecol(gb, window->cursor, &cursorline, &cursorcol);

  bool number = option_get_bool("number");
  bool relativenumber = option_get_bool("relativenumber");

  int numberwidth = window_numberwidth(window);

  int w = window->w - numberwidth;
  int h = window->h;

  int topy = window->top;
  int topx = window->left;
  int rows = min(gb->lines->len - topy, h);
  for (int y = 0; y < rows; ++y) {
    int absolute = window->top + y + 1;
    int relative = abs(absolute - cursorline - 1);
    int linenumber = absolute;
    if (number || relativenumber) {
      if (relativenumber && !(number && relative == 0)) {
        linenumber = relative;
      }

      int col = numberwidth - 2;
      do {
        int digit = linenumber % 10;
        tb_change_cell(col--, y, digit + '0', TB_YELLOW, TB_DEFAULT);
        linenumber = (linenumber - digit) / 10;
      } while (linenumber > 0);
    }

    bool drawcursorline = relative == 0 && option_get_bool("cursorline");
    uint16_t fg = drawcursorline ? (TB_WHITE | TB_UNDERLINE) : TB_WHITE;

    int cols = min(gb->lines->buf[y + topy] - topx, w);
    for (int x = 0; x < cols; ++x) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, y + topy, x + topx));
      window_change_cell(window, x, y, c, fg, TB_DEFAULT);
    }

    if (drawcursorline) {
      for (int x = cols; x < w; ++x) {
        window_change_cell(window, x, y, ' ', fg, TB_DEFAULT);
      }
    }
  }

  window_draw_cursor(window);
}
