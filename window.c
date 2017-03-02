#include "window.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "list.h"
#include "options.h"
#include "util.h"

struct window_t *window_create(struct buffer_t *buffer, size_t w, size_t h) {
  struct window_t *window = xmalloc(sizeof(*window));

  window->split_type = WINDOW_LEAF;
  window->parent = NULL;

  window->buffer = NULL;
  window_set_buffer(window, buffer);
  window->visual_mode_anchor = NULL;

  window->w = w;
  window->h = h;

  window->tag_stack = list_create();
  window->tag = NULL;

  return window;
}

static struct window_t *window_sibling(struct window_t *window) {
  assert(window->parent);
  if (window == window->parent->split.first) {
    return window->parent->split.second;
  }
  assert(window == window->parent->split.second);
  return window->parent->split.first;
}

static size_t window_w(struct window_t *window) {
  if (!window->parent) {
    return window->w;
  }

  if (window->parent->split_type == WINDOW_SPLIT_HORIZONTAL) {
    return window_w(window->parent);
  }

  if (window == window->parent->split.first) {
    return window->parent->split.point;
  }
  return window_w(window->parent) - window->parent->split.point;
}

static size_t window_h(struct window_t *window) {
  if (!window->parent) {
    return window->h;
  }

  if (window->parent->split_type == WINDOW_SPLIT_VERTICAL) {
    return window_h(window->parent);
  }

  if (window == window->parent->split.first) {
    return window->parent->split.point;
  }
  return window_h(window->parent) - window->parent->split.point;
}

static size_t window_x(struct window_t *window) {
  if (!window->parent) {
    return 0;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_HORIZONTAL:
    return window_x(window->parent);
  case WINDOW_SPLIT_VERTICAL:
    if (window == window->parent->split.first) {
      return window_x(window->parent);
    }
    return window_x(window->parent) + window_w(window_sibling(window));
  case WINDOW_LEAF:
    assert(0);
    return 0;
  }
}

static size_t window_y(struct window_t *window) {
  if (!window->parent) {
    return 0;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_VERTICAL:
    return window_y(window->parent);
  case WINDOW_SPLIT_HORIZONTAL:
    if (window == window->parent->split.first) {
      return window_y(window->parent);
    }
    return window_y(window->parent) + window_h(window_sibling(window));
  case WINDOW_LEAF:
    assert(0);
    return 0;
  }
}

struct window_t *window_split(struct window_t *window,
                              enum window_split_type_t type) {
  struct window_t *copy = xmalloc(sizeof(*window));
  memcpy(copy, window, sizeof(*window));

  struct window_t *sibling = window_create(
      window->buffer, window->w, window->h);

  copy->parent = window;
  sibling->parent = window;

  window->split_type = type;

  switch (type) {
  case WINDOW_SPLIT_VERTICAL:
    window->split.point = window_w(window) / 2;
    if (option_get_bool("splitright")) {
      window->split.first = copy;
      window->split.second = sibling;
    } else {
      window->split.first = sibling;
      window->split.second = copy;
    }
    break;
  case WINDOW_SPLIT_HORIZONTAL:
    window->split.point = window_h(window) / 2;
    if (option_get_bool("splitbelow")) {
      window->split.first = copy;
      window->split.second = sibling;
    } else {
      window->split.first = sibling;
      window->split.second = copy;
    }
    break;
  case WINDOW_LEAF:
    assert(0);
  }

  return sibling;
}

// FIXME(ibadawi): Enforce minimum size for a window
void window_resize(struct window_t *window, int dw, int dh) {
  if (!window->parent) {
    return;
  }

  size_t *point = &window->parent->split.point;

  if (dw) {
    if (window->parent->split_type == WINDOW_SPLIT_VERTICAL) {
      if (window == window->parent->split.first) {
        *point = (size_t)((int)*point + dw);
      } else {
        assert(window == window->parent->split.second);
        *point = (size_t)((int)*point - dw);
      }
    } else {
      assert(window->parent->split_type == WINDOW_SPLIT_HORIZONTAL);
      struct window_t *parent = window->parent;
      while (parent && parent->split_type != WINDOW_SPLIT_VERTICAL) {
        window = parent;
        parent = parent->parent;
      }
      if (parent) {
        assert(parent->split_type == WINDOW_SPLIT_VERTICAL);
        window_resize(window, dw, 0);
      }
    }
  }

  if (dh) {
    if (window->parent->split_type == WINDOW_SPLIT_HORIZONTAL) {
      if (window == window->parent->split.first) {
        *point = (size_t)((int)*point + dh);
      } else {
        assert(window == window->parent->split.second);
        *point = (size_t)((int)*point - dh);
      }
    } else {
      assert(window->parent->split_type == WINDOW_SPLIT_VERTICAL);
      struct window_t *parent = window->parent;
      while (parent && parent->split_type != WINDOW_SPLIT_HORIZONTAL) {
        window = parent;
        parent = parent->parent;
      }
      if (parent) {
        assert(parent->split_type == WINDOW_SPLIT_HORIZONTAL);
        window_resize(window, 0, dh);
      }
    }
  }
}

void window_set_buffer(struct window_t *window, struct buffer_t* buffer) {
  if (window->buffer) {
    list_remove(window->buffer->marks, window->cursor);
    free(window->cursor);
  }

  window->buffer = buffer;
  window->top = 0;
  window->left = 0;
  window->cursor = region_create(0, 1);
  list_append(buffer->marks, window->cursor);
}

size_t window_cursor(struct window_t *window) {
  return window->cursor->start;
}

static struct window_t *window_first_leaf(struct window_t *window) {
  while (window->split_type != WINDOW_LEAF) {
    window = window->split.first;
  }
  return window;
}

static struct window_t *window_last_leaf(struct window_t *window) {
  while (window->split_type != WINDOW_LEAF) {
    window = window->split.second;
  }
  return window;
}

struct window_t *window_root(struct window_t *window) {
  while (window->parent) {
    window = window->parent;
  }
  return window;
}

struct window_t *window_left(struct window_t *window) {
  if (!window || !window->parent) {
    return NULL;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_HORIZONTAL:
    return window_left(window->parent);
  case WINDOW_SPLIT_VERTICAL:
    if (window == window->parent->split.second) {
      return window_last_leaf(window_sibling(window));
    }
    return window_left(window->parent);
  case WINDOW_LEAF:
    assert(0);
    return NULL;
  }
}

struct window_t *window_right(struct window_t *window) {
  if (!window || !window->parent) {
    return NULL;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_HORIZONTAL:
    return window_right(window->parent);
  case WINDOW_SPLIT_VERTICAL:
    if (window == window->parent->split.first) {
      return window_first_leaf(window_sibling(window));
    }
    return window_right(window->parent);
  case WINDOW_LEAF:
    assert(0);
    return NULL;
  }
}

struct window_t *window_up(struct window_t *window) {
  if (!window || !window->parent) {
    return NULL;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_VERTICAL:
    return window_up(window->parent);
  case WINDOW_SPLIT_HORIZONTAL:
    if (window == window->parent->split.second) {
      return window_last_leaf(window_sibling(window));
    }
    return window_up(window->parent);
  case WINDOW_LEAF:
    assert(0);
    return NULL;
  }
}

struct window_t *window_down(struct window_t *window) {
  if (!window || !window->parent) {
    return NULL;
  }

  switch (window->parent->split_type) {
  case WINDOW_SPLIT_VERTICAL:
    return window_down(window->parent);
  case WINDOW_SPLIT_HORIZONTAL:
    if (window == window->parent->split.first) {
      return window_first_leaf(window_sibling(window));
    }
    return window_down(window->parent);
  case WINDOW_LEAF:
    assert(0);
    return NULL;
  }
}

static void window_free(struct window_t *window) {
  if (window->split_type == WINDOW_LEAF) {
    list_free(window->tag_stack, free);
    if (window->buffer) {
      list_remove(window->buffer->marks, window->cursor);
      free(window->cursor);
    }
  }
  free(window);
}

struct window_t *window_close(struct window_t *window) {
  assert(window->split_type == WINDOW_LEAF);

  struct window_t *parent = window->parent;
  struct window_t *sibling = window_sibling(window);
  bool was_left_child = window == window->parent->split.first;

  enum window_split_type_t old_parent_type = parent->split_type;
  struct window_t *grandparent = parent->parent;
  size_t w = parent->w;
  size_t h = parent->h;

  memcpy(parent, sibling, sizeof(*sibling));
  parent->parent = grandparent;
  parent->w = w;
  parent->h = h;

  if (parent->split_type != WINDOW_LEAF) {
    parent->split.first->parent = parent;
    parent->split.second->parent = parent;
  }

  if (was_left_child && old_parent_type == parent->split_type) {
    if (parent->split_type == WINDOW_SPLIT_HORIZONTAL) {
      parent->split.point = window_h(parent) - parent->split.point;
    } else if (parent->split_type == WINDOW_SPLIT_VERTICAL) {
      parent->split.point = window_w(parent) - parent->split.point;
    }
  }

  window_free(window);
  // free, not window_free, because that would free parent's fields.
  free(sibling);

  return window_first_leaf(parent);
}

void window_set_cursor(struct window_t *window, size_t pos) {
  window->cursor->start = pos;
  window->cursor->end = pos + 1;
}

// Number of columns to use for the line number (including the trailing space).
// TODO(isbadawi): This might not be exactly correct for relativenumber mode.
static size_t window_numberwidth(struct window_t* window) {
  if (!option_get_bool("number") && !option_get_bool("relativenumber")) {
    return 0;
  }

  size_t nlines = window->buffer->text->lines->len;
  char buf[10];
  size_t maxwidth = (size_t) snprintf(buf, 10, "%zu", nlines);

  return max((size_t) option_get_int("numberwidth"), maxwidth + 1);
}

static bool window_should_draw_plate(struct window_t *window) {
  return window->parent != NULL;
}

static void window_ensure_cursor_visible(struct window_t *window) {
  size_t x, y;
  gb_pos_to_linecol(window->buffer->text, window_cursor(window), &y, &x);

  size_t w = window_w(window) - window_numberwidth(window);
  size_t h = window_h(window);
  if (window_right(window)) {
    --w;
  }
  if (window_should_draw_plate(window)) {
    --h;
  }

  // TODO(isbadawi): Clean this up...
  // The problem is that x & w (and y & h) are unsigned, but we want to treat
  // their difference as signed. All these casts are necessary to get around
  // warnings.
  window->left = (size_t) ((ssize_t) max((ssize_t) min(window->left, x), (ssize_t) (x - w + 1)));
  window->top = (size_t) ((ssize_t) max((ssize_t) min(window->top, y), (ssize_t) (y - h + 1)));
}

static void window_change_cell(struct window_t *window, size_t x, size_t y, char c,
                               int fg, int bg) {
  tb_change_cell(
      (int) (window_numberwidth(window) + window_x(window) + x),
      (int) (window_y(window) + y),
      (uint32_t) c, (uint16_t) fg, (uint16_t) bg);
}

void window_draw_cursor(struct window_t *window) {
  size_t cursor = window_cursor(window);
  struct gapbuf_t *gb = window->buffer->text;
  char c = gb_getchar(gb, cursor);
  size_t x, y;
  gb_pos_to_linecol(gb, cursor, &y, &x);

  window_change_cell(window,
      x - window->left,
      y - window->top,
      c == '\n' ? ' ' : c,
      TB_BLACK,
      TB_WHITE);
}

static void window_draw_plate(struct window_t *window) {
  char plate[300];
  strcpy(plate, *window->buffer->name ? window->buffer->name : "[No Name]");
  if (window->buffer->dirty) {
    strcat(plate, " [+]");
  }
  size_t platelen = strlen(plate);
  for (size_t x = 0; x < window_w(window); ++x) {
    char c = x < platelen ? plate[x] : ' ';
    tb_change_cell(
        (int) (window_x(window) + x),
        (int) (window_y(window) + window_h(window) - 1),
        (uint32_t) c, TB_BLACK, TB_WHITE);
  }
}

void window_draw(struct window_t *window) {
  if (window->split_type != WINDOW_LEAF) {
    window_draw(window->split.first);
    window_draw(window->split.second);
    if (window->split_type == WINDOW_SPLIT_VERTICAL) {
      int x = (int) window_x(window->split.second) - 1;
      for (size_t y = 0; y < window_h(window->split.first); ++y) {
        tb_change_cell(x, (int) (window_y(window->split.first) + y),
            '|', TB_BLACK, TB_WHITE);
      }
    }
    return;
  }

  window_ensure_cursor_visible(window);
  struct gapbuf_t *gb = window->buffer->text;

  size_t cursorline, cursorcol;
  gb_pos_to_linecol(gb, window_cursor(window), &cursorline, &cursorcol);

  bool number = option_get_bool("number");
  bool relativenumber = option_get_bool("relativenumber");

  size_t numberwidth = window_numberwidth(window);

  size_t w = window_w(window) - numberwidth;
  size_t h = window_h(window);

  size_t topy = window->top;
  size_t topx = window->left;
  size_t rows = min(gb->lines->len - topy, h);

  struct region_t selection;
  if (window->visual_mode_anchor) {
    region_set(&selection,
        window->cursor->start, window->visual_mode_anchor->start);
  }

  for (size_t y = 0; y < rows; ++y) {
    size_t absolute = window->top + y + 1;
    size_t relative = (size_t) labs((ssize_t)(absolute - cursorline - 1));
    size_t linenumber = absolute;
    if (number || relativenumber) {
      if (relativenumber && !(number && relative == 0)) {
        linenumber = relative;
      }

      size_t col = window_x(window) + numberwidth - 2;
      do {
        size_t digit = linenumber % 10;
        tb_change_cell((int) col--, (int) (window_y(window) + y),
                       (uint32_t) (digit + '0'), TB_YELLOW, TB_DEFAULT);
        linenumber = (linenumber - digit) / 10;
      } while (linenumber > 0);
    }

    bool drawcursorline = relative == 0 &&
      option_get_bool("cursorline") &&
      !window->visual_mode_anchor;

    size_t cols = (size_t) max(0, min((ssize_t) gb->lines->buf[y + topy] - (ssize_t) topx, (ssize_t) w));
    for (size_t x = 0; x < cols; ++x) {
      int fg = TB_WHITE;
      int bg = TB_DEFAULT;
      size_t pos = gb_linecol_to_pos(gb, y + topy, x + topx);
      if (window->visual_mode_anchor &&
          selection.start <= pos && pos < selection.end) {
        fg = TB_BLACK;
        bg = TB_WHITE;
      }

      if (drawcursorline) {
        fg |= TB_UNDERLINE;
      }

      char c = gb_getchar(gb, pos);
      window_change_cell(window, x, y, c, fg, bg);
    }

    if (drawcursorline) {
      for (size_t x = cols; x < w; ++x) {
        window_change_cell(window, x, y, ' ', TB_WHITE | TB_UNDERLINE, TB_DEFAULT);
      }
    }
  }

  size_t nlines = gb_nlines(window->buffer->text);
  for (size_t y = nlines; y < window_h(window); ++y) {
    tb_change_cell(
        (int) window_x(window),
        (int) (window_y(window) + y),
        '~', TB_BLUE, TB_DEFAULT);
  }

  if (window_should_draw_plate(window)) {
    window_draw_plate(window);
  }
}
