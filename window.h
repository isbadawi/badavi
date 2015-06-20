#pragma once

#include <stddef.h>

struct window_t {
  // The buffer being edited.
  struct buffer_t *buffer;
  // The position and size of the window.
  size_t x;
  size_t y;
  size_t w;
  size_t h;
  // The coordinates of the top left cell visible on screen.
  size_t top;
  size_t left;

  // The offset of the cursor.
  struct region_t *cursor;

  struct list_t *tag_stack;
  struct tag_jump_t *tag;
};

struct window_t *window_create(struct buffer_t *buffer, size_t x, size_t y, size_t w, size_t h);
void window_set_buffer(struct window_t *window, struct buffer_t *buffer);

size_t window_cursor(struct window_t *window);
void window_set_cursor(struct window_t *window, size_t pos);

void window_draw(struct window_t *window);
void window_draw_cursor(struct window_t *window);
