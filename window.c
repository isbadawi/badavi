#include "window.h"

#include <stdlib.h>

#include <termbox.h>

#include "util.h"

window_t *window_create(buffer_t *buffer, int x, int y, int w, int h) {
  window_t *window = malloc(sizeof(window_t));
  if (!window) {
    return NULL;
  }

  window->next = NULL;
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

static void window_ensure_cursor_visible(window_t *window) {
  int x, y;
  gb_pos_to_linecol(window->buffer->text, window->cursor, &y, &x);

  window->left = max(min(window->left, x), x - window->w + 1);
  window->top = max(min(window->top, y), y - window->h + 1);
}

static void window_draw_cursor(window_t *window) {
  gapbuf_t *gb = window->buffer->text;
  char c = gb_getchar(gb, window->cursor);
  int x, y;
  gb_pos_to_linecol(gb, window->cursor, &y, &x);

  tb_change_cell(
      window->x + x - window->left,
      window->y + y - window->top,
      c == '\n' ? ' ' : c,
      TB_DEFAULT,
      TB_WHITE);
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);
  gapbuf_t *gb = window->buffer->text;

  int topy = window->top;
  int topx = window->left;
  int rows = min(gb->lines->len - topy, window->h);
  for (int y = 0; y < rows; ++y) {
    int cols = min(gb->lines->buf[y + topy] - topx, window->w);
    for (int x = 0; x < cols; ++x) {
      char c = gb_getchar(gb, gb_linecol_to_pos(gb, y + topy, x + topx));
      tb_change_cell(window->x + x, window->y + y, c, TB_WHITE, TB_DEFAULT);
    }
  }

  window_draw_cursor(window);
}
