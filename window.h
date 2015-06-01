#pragma once

#include <stddef.h>

#include "buffer.h"
#include "list.h"
#include "tags.h"

typedef struct {
  buffer_t *buffer;
  size_t cursor;
  tag_t *tag;
} tag_jump_t;

typedef struct window_t {
  // The buffer being edited.
  buffer_t *buffer;
  // The position and size of the window.
  size_t x;
  size_t y;
  size_t w;
  size_t h;
  // The coordinates of the top left cell visible on screen.
  size_t top;
  size_t left;
  // The offset of the cursor.
  size_t cursor;

  list_t *tag_stack;
  tag_jump_t *tag;
} window_t;

window_t *window_create(buffer_t *buffer, size_t x, size_t y, size_t w, size_t h);
void window_draw(window_t *window);
void window_draw_cursor(window_t *window);
