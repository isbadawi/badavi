#pragma once

#include <stddef.h>

struct window {
  struct window *parent;
  enum window_split_type {
    WINDOW_LEAF,
    WINDOW_SPLIT_VERTICAL,
    WINDOW_SPLIT_HORIZONTAL
  } split_type;

  // The size of the window. Only valid for the root window.
  size_t w;
  size_t h;

  union {
    struct {
      // The buffer being edited.
      struct buffer *buffer;
      // The coordinates of the top left cell visible on screen.
      size_t top;
      size_t left;

      // The offset of the cursor.
      struct region *cursor;

      // The visual mode selection.
      // NULL if not in visual mode.
      struct region *visual_mode_selection;

      struct list *tag_stack;
      struct tag_jump *tag;
    };

    struct {
      struct window *first;
      struct window *second;
      size_t point;
    } split;
  };
};

struct window *window_create(struct buffer *buffer, size_t w, size_t h);
struct window *window_close(struct window *window);
struct window *window_split(struct window *window,
                              enum window_split_type type);

void window_resize(struct window *window, int dw, int dh);
void window_equalize(struct window *window,
                     enum window_split_type type);

struct window *window_root(struct window *window);
struct window *window_left(struct window *window);
struct window *window_right(struct window *window);
struct window *window_up(struct window *window);
struct window *window_down(struct window *window);

struct window *window_first_leaf(struct window *window);

void window_set_buffer(struct window *window, struct buffer *buffer);

size_t window_cursor(struct window *window);
void window_set_cursor(struct window *window, size_t pos);

void window_draw(struct window *window);
void window_draw_cursor(struct window *window);

size_t window_w(struct window *window);
size_t window_h(struct window *window);
size_t window_x(struct window *window);
size_t window_y(struct window *window);

void window_get_ruler(struct window *window, char *buf, size_t buflen);
