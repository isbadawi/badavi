#include "window.h"

#include <stdlib.h>

#include <termbox.h>

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
  gb_pos_to_linecol(gb, window->cursor, &cursorx, &cursory);
  gb_pos_to_linecol(gb, window->top, &topx, &topy);
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

  window->top = gb_linecol_to_pos(gb, topx, topy);
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  int y = 0;
  int w = tb_width();
  int h = tb_height();
  int topx, topy;
  int cursorx, cursory;
  gb_pos_to_linecol(gb, window->top, &topx, &topy);
  gb_pos_to_linecol(gb, window->cursor, &cursorx, &cursory);
  // TODO(isbadawi): Each linecol_to_pos call is a loop.
  for (int j = topy; j <= gb->lines->len && y < h - 1; j++) {
    int x = 0;
    for (int i = 0; i < gb->lines->buf[j] && x < w; ++i) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, j, i));
      tb_change_cell(x++, y, c, TB_WHITE, TB_DEFAULT);
    }
    if (j == cursory) {
      int x = cursorx - topx;
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, j, x));
      tb_change_cell(x, y, c, TB_DEFAULT, TB_WHITE);
    }
    y++;
  }
}
