#ifndef _window_h_included
#define _window_h_included

#include "buffer.h"

typedef struct window_t {
  // The buffer being edited.
  buffer_t *buffer;
  // The position and size of the window.
  int x;
  int y;
  int w;
  int h;
  // The coordinates of the top left cell visible on screen.
  int top;
  int left;
  // The offset of the cursor.
  int cursor;
  // The next window, or NULL.
  struct window_t *next;
} window_t;

window_t *window_create(buffer_t *buffer, int x, int y, int w, int h);

void window_draw(window_t *window);

#endif
