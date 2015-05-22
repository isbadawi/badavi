#ifndef _window_h_included
#define _window_h_included

#include <stddef.h>

#include "buffer.h"

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
} window_t;

window_t *window_create(buffer_t *buffer, size_t x, size_t y, size_t w, size_t h);

void window_draw(window_t *window);

#endif
