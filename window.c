#include "window.h"

#include <stdlib.h>

#include <termbox.h>

#include "util.h"

window_t *window_create(buffer_t *buffer) {
  window_t *window = malloc(sizeof(window_t));
  if (!window) {
    return NULL;
  }

  window->next = NULL;
  window->buffer = buffer;
  window->top = 0;
  window->left = 0;
  window->cursor = 0;

  return window;
}

static void window_ensure_cursor_visible(window_t *window) {
  int w = tb_width();
  int h = tb_height();
  gapbuf_t *gb = window->buffer->text;

  // TODO(isbadawi): This might be expensive for buffers with many lines.
  int cursorx, cursory;
  gb_pos_to_linecol(gb, window->cursor, &cursory, &cursorx);

  if (cursorx < window->left) {
    window->left = cursorx;
  } else if (cursorx - window->left >= w) {
    window->left = cursorx - w + 1;
  }

  if (cursory < window->top) {
    window->top = cursory;
  } else if (cursory - window->top >= h - 1) {
    window->top = cursory - h + 2;
  }
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  int w = tb_width();
  int h = tb_height();
  int topy = window->top;
  int topx = window->left;
  // TODO(isbadawi): Each linecol_to_pos call is a loop.
  for (int y = topy; y <= gb->lines->len && y - topy < h - 1; ++y) {
    for (int x = topx; x < gb->lines->buf[y] && x - topx < w; ++x) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, y, x));
      tb_change_cell(x - topx, y - topy, c, TB_WHITE, TB_DEFAULT);
    }
  }

  char c = gb_getchar(gb, window->cursor);
  c = c == '\n' ? ' ' : c;
  int cursorx, cursory;
  gb_pos_to_linecol(gb, window->cursor, &cursory, &cursorx);
  tb_change_cell(cursorx - topx, cursory - topy, c, TB_DEFAULT, TB_WHITE);
}
