#ifndef _window_h_included
#define _window_h_included

#include "buffer.h"

// A line & column pair.
typedef struct {
  // The line (a linked list node).
  line_t *line;
  // The offset within that line.
  int offset;
} pos_t;

typedef struct window_t {
  // The buffer being edited.
  buffer_t *buffer;
  // The position of the top left cell visible on screen.
  pos_t top;
  // The cursor.
  pos_t cursor;
  // The next window, or NULL.
  struct window_t *next;
} window_t;

window_t *window_create(buffer_t *buffer);

// TODO(isbadawi): This could take x, y, w, h to support split windows.
void window_draw(window_t *window);

#endif
