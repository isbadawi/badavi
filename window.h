#pragma once

#include <stddef.h>

struct window_t {
  struct window_t *parent;
  enum window_split_type_t {
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
      struct buffer_t *buffer;
      // The coordinates of the top left cell visible on screen.
      size_t top;
      size_t left;

      // The offset of the cursor.
      struct region_t *cursor;

      // The offset of the cursor when visual mode was entered.
      struct region_t *visual_mode_anchor;

      struct list_t *tag_stack;
      struct tag_jump_t *tag;
    };

    struct {
      struct window_t *first;
      struct window_t *second;
      size_t point;
    } split;
  };
};

struct window_t *window_create(struct buffer_t *buffer, size_t w, size_t h);
struct window_t *window_close(struct window_t *window);
struct window_t *window_split(struct window_t *window,
                              enum window_split_type_t type);

void window_resize(struct window_t *window, int dw, int dh);
void window_equalize(struct window_t *window,
                     enum window_split_type_t type);

struct window_t *window_root(struct window_t *window);
struct window_t *window_left(struct window_t *window);
struct window_t *window_right(struct window_t *window);
struct window_t *window_up(struct window_t *window);
struct window_t *window_down(struct window_t *window);

void window_set_buffer(struct window_t *window, struct buffer_t *buffer);

size_t window_cursor(struct window_t *window);
void window_set_cursor(struct window_t *window, size_t pos);

void window_draw(struct window_t *window);
void window_draw_cursor(struct window_t *window);
