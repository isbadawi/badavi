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
  int h = tb_height() - 1;

  int x, y;
  gb_pos_to_linecol(window->buffer->text, window->cursor, &x, &y);

  window->left = max(min(window->left, x), x - w + 1);
  window->top = max(min(window->top, y), y - h + 1);
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  int w = tb_width();
  int h = tb_height() - 1;
  int topy = window->top;
  int topx = window->left;
  int rows = min(gb->lines->len - topy, h);
  for (int y = 0; y < rows; ++y) {
    int cols = min(gb->lines->buf[y + topy] - topx, w);
    for (int x = 0; x < cols; ++x) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, y + topy, x + topx));
      tb_change_cell(x, y, c, TB_WHITE, TB_DEFAULT);
    }
  }

  char c = gb_getchar(gb, window->cursor);
  c = c == '\n' ? ' ' : c;
  int cursorx, cursory;
  gb_pos_to_linecol(gb, window->cursor, &cursory, &cursorx);
  tb_change_cell(cursorx - topx, cursory - topy, c, TB_DEFAULT, TB_WHITE);
}
