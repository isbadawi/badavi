#pragma once

#include <stddef.h>

#include <sys/queue.h>

#include "options.h"
#include "util.h"

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

  struct {
#define OPTION(name, type, _) type name;
  WINDOW_OPTIONS
#undef OPTION
  } opt;

  union {
    struct {
      // The buffer being edited.
      struct buffer *buffer;

      char *alternate_path;

      // The coordinates of the top left cell visible on screen.
      size_t top;
      size_t left;

      // Window-local working directory if set (otherwise NULL).
      char *pwd;

      // The offset of the cursor.
      struct mark *cursor;

      // The incremental match if 'incsearch' is enabled.
      bool have_incsearch_match;
      struct region incsearch_match;

      // The visual mode selection.
      // NULL if not in visual mode.
      struct region *visual_mode_selection;

      TAILQ_HEAD(tag_list, tag_jump) tag_stack;
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
void window_free(struct window *window);

// Closes the current window and returns a pointer to the "next" window.
// Caller should use window_free on passed-in window afterwards.
struct window *window_close(struct window *window);

enum window_split_direction {
  WINDOW_SPLIT_LEFT,
  WINDOW_SPLIT_RIGHT,
  WINDOW_SPLIT_ABOVE,
  WINDOW_SPLIT_BELOW
};

struct window *window_split(struct window *window,
                              enum window_split_direction direction);

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
void window_center_cursor(struct window *window);

size_t window_w(struct window *window);
size_t window_h(struct window *window);
size_t window_x(struct window *window);
size_t window_y(struct window *window);

void window_page_up(struct window *window);
void window_page_down(struct window *window);

void window_clear_working_directories(struct window *window);
