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

  if (buffer) {
    window->top.line = buffer->head->next;
    window->top.offset = 0;
    window->cursor.line = window->top.line;
    window->cursor.offset = 0;
    window->next = NULL;
  }

  return window;
}

static void window_ensure_cursor_visible(window_t *window) {
  int w = tb_width();
  int h = tb_height();
  int x = window->cursor.offset - window->top.offset;
  // TODO(isbadawi): This might be expensive for buffers with many lines.
  int y = buffer_index_of_line(window->buffer, window->cursor.line) -
    buffer_index_of_line(window->buffer, window->top.line);

  if (x < 0) {
    window->top.offset += x;
  } else if (x >= w) {
    window->top.offset -= w - x - 1;
  }

  while (y >= h - 1) {
    window->top.line = window->top.line->next;
    y--;
  }

  while (y < 0) {
    window->top.line = window->top.line->prev;
    y++;
  }
}

void window_draw(window_t *window) {
  window_ensure_cursor_visible(window);

  int y = 0;
  int w = tb_width();
  int h = tb_height();
  for (line_t *line = window->top.line; line && y < h - 1; line = line->next) {
    int x = 0;
    for (int i = window->top.offset; i < line->buf->len && x < w; ++i) {
      tb_change_cell(x++, y, line->buf->buf[i], TB_WHITE, TB_DEFAULT);
    }
    if (line == window->cursor.line) {
      int x = window->cursor.offset - window->top.offset;
      tb_change_cell(x, y, line->buf->buf[x], TB_DEFAULT, TB_WHITE);
    }
    y++;
  }
}
