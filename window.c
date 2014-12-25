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
  window->cursor = 0;

  return window;
}

static void window_ensure_cursor_visible(window_t *window) {
  int w = tb_width();
  int h = tb_height();
  gapbuf_t *gb = window->buffer->text;

  // TODO(isbadawi): This might be expensive for buffers with many lines.
  int cursorx, cursory;
  int topx, topy;
  gb_pos_to_linecol(gb, window->cursor, &cursory, &cursorx);
  gb_pos_to_linecol(gb, window->top, &topy, &topx);

  int x = cursorx - topx;
  int y = cursory - topy;

  if (x < 0) {
    topx += x;
  } else if (x >= w) {
    topx -= w - x - 1;
  }

  if (y < 0) {
    topy -= y;
  } else if (y >= h - 1) {
    topy += h - y - 2;
  }

  window->top = gb_linecol_to_pos(gb, topy, topx);
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  int w = tb_width();
  int h = tb_height();
  int topx, topy;
  gb_pos_to_linecol(gb, window->top, &topy, &topx);
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
