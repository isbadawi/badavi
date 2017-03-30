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

struct window *window_create(struct buffer *buffer, size_t w, size_t h) {
  struct window *window = xmalloc(sizeof(*window));

  window->split_type = WINDOW_LEAF;
  window->parent = NULL;

  window->buffer = NULL;
  window_set_buffer(window, buffer);
  window->visual_mode_selection = NULL;

  window->w = w;
  window->h = h;

  window->tag_stack = list_create();
  window->tag = NULL;

  return window;
}

static struct window *window_sibling(struct window *window) {
  assert(window->parent);
  if (window == window->parent->split.first) {
    return window->parent->split.second;
  }
  assert(window == window->parent->split.second);
  return window->parent->split.first;
}

size_t window_w(struct window *window) {
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

size_t window_h(struct window *window) {
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

size_t window_x(struct window *window) {
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
  }

  return 0;
}

size_t window_y(struct window *window) {
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
  }

  return 0;
}

static size_t window_count_splits(struct window *window,
                                  enum window_split_type type) {
  if (window->split_type == WINDOW_LEAF) {
    return 0;
  }

  size_t lhs = window_count_splits(window->split.first, type);
  size_t rhs = window_count_splits(window->split.second, type);

  if (window->split_type == type) {
    return 1 + lhs + rhs;
  }
  return max(lhs, rhs);
}

static size_t window_count_leaves(struct window *window,
                                  enum window_split_type type) {

  if (window->split_type == WINDOW_LEAF) {
    return window->parent->split_type == type ? 1 : 0;
  }

  size_t lhs = window_count_leaves(window->split.first, type);
  size_t rhs = window_count_leaves(window->split.second, type);

  if (window->split_type == type) {
    return lhs + rhs;
  }
  return max(lhs, rhs);
}

static void window_set_split_size(struct window *window,
                                  enum window_split_type type,
                                  size_t size) {
  if (window->split_type == WINDOW_LEAF) {
    return;
  }

  if (window->split_type == type) {
    window->split.point =
      size * max(1, window_count_leaves(window->split.first, type));
  }
  window_set_split_size(window->split.first, type, size);
  window_set_split_size(window->split.second, type, size);
}

void window_equalize(struct window *window,
                     enum window_split_type type) {
  if (window->split_type == WINDOW_LEAF) {
    window = window->parent;
  }
  if (!window) {
    return;
  }

  struct window *root = window_root(window);

  size_t n = window_count_splits(root, type);
  size_t size;
  if (type == WINDOW_SPLIT_VERTICAL) {
    size = root->w;
  } else {
    size = root->h;
  }

  window_set_split_size(root, type, size / (n + 1));
}

struct window *window_split(struct window *window,
                            enum window_split_direction direction) {
  assert(window->split_type == WINDOW_LEAF);

  struct window *copy = xmalloc(sizeof(*window));
  memcpy(copy, window, sizeof(*window));

  struct window *sibling = window_create(
      window->buffer, window->w, window->h);

  copy->parent = window;
  sibling->parent = window;

  switch (direction) {
  case WINDOW_SPLIT_LEFT:
    window->split_type = WINDOW_SPLIT_VERTICAL;
    window->split.point = window_w(window) / 2;
    window->split.first = sibling;
    window->split.second = copy;
    break;
  case WINDOW_SPLIT_RIGHT:
    window->split_type = WINDOW_SPLIT_VERTICAL;
    window->split.point = window_w(window) / 2;
    window->split.first = copy;
    window->split.second = sibling;
    break;
  case WINDOW_SPLIT_ABOVE:
    window->split_type = WINDOW_SPLIT_HORIZONTAL;
    window->split.point = window_h(window) / 2;
    window->split.first = sibling;
    window->split.second = copy;
    break;
  case WINDOW_SPLIT_BELOW:
    window->split_type = WINDOW_SPLIT_HORIZONTAL;
    window->split.point = window_h(window) / 2;
    window->split.first = copy;
    window->split.second = sibling;
  }

  if (option_get_bool("equalalways")) {
    window_equalize(window, window->split_type);
  }

  return sibling;
}

// FIXME(ibadawi): Enforce minimum size for a window
void window_resize(struct window *window, int dw, int dh) {
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
      struct window *parent = window->parent;
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
      struct window *parent = window->parent;
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

void window_set_buffer(struct window *window, struct buffer* buffer) {
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

size_t window_cursor(struct window *window) {
  return window->cursor->start;
}

struct window *window_first_leaf(struct window *window) {
  while (window->split_type != WINDOW_LEAF) {
    window = window->split.first;
  }
  return window;
}

static struct window *window_last_leaf(struct window *window) {
  while (window->split_type != WINDOW_LEAF) {
    window = window->split.second;
  }
  return window;
}

struct window *window_root(struct window *window) {
  while (window->parent) {
    window = window->parent;
  }
  return window;
}

struct window *window_left(struct window *window) {
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
  }

  return NULL;
}

struct window *window_right(struct window *window) {
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
  }

  return NULL;
}

struct window *window_up(struct window *window) {
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
  }

  return NULL;
}

struct window *window_down(struct window *window) {
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
  }

  return NULL;
}

static void window_free(struct window *window) {
  if (window->split_type == WINDOW_LEAF) {
    list_free(window->tag_stack, free);
    if (window->buffer) {
      list_remove(window->buffer->marks, window->cursor);
      free(window->cursor);
    }
  }
  free(window);
}

struct window *window_close(struct window *window) {
  assert(window->split_type == WINDOW_LEAF);

  struct window *parent = window->parent;
  struct window *sibling = window_sibling(window);
  bool was_left_child = window == window->parent->split.first;

  enum window_split_type old_parent_type = parent->split_type;
  struct window *grandparent = parent->parent;
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

  if (option_get_bool("equalalways")) {
    window_equalize(parent, old_parent_type);
  }

  window_free(window);
  // free, not window_free, because that would free parent's fields.
  free(sibling);

  return window_first_leaf(parent);
}

void window_set_cursor(struct window *window, size_t pos) {
  window->cursor->start = pos;
  window->cursor->end = pos + 1;
}

// Number of columns to use for the line number (including the trailing space).
// TODO(isbadawi): This might not be exactly correct for relativenumber mode.
static size_t window_numberwidth(struct window* window) {
  if (!option_get_bool("number") && !option_get_bool("relativenumber")) {
    return 0;
  }

  size_t nlines = window->buffer->text->lines->len;
  char buf[10];
  size_t maxwidth = (size_t) snprintf(buf, 10, "%zu", nlines);

  return max((size_t) option_get_int("numberwidth"), maxwidth + 1);
}

static bool window_should_draw_plate(struct window *window) {
  return window->parent != NULL;
}

static void window_ensure_cursor_visible(struct window *window) {
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

  if (y < window->top) {
    window->top = y;
  } else if (y > window->top + h - 1) {
    window->top = y - h + 1;
  }

  if (x < window->left) {
    window->left = x;
  } else if (x > window->left + w - 1) {
    window->left = x - w + 1;
  }
}

static struct tb_cell *window_get_cell(struct window *window, size_t x, size_t y) {
  struct tb_cell *cells = tb_cell_buffer();
  size_t offset = (window_y(window) + y) * (size_t) tb_width() +
    window_x(window) + x;
  return &cells[offset];
}

static void window_change_cell(struct window *window, size_t x, size_t y, char c,
                               int fg, int bg) {
  struct tb_cell *cell = window_get_cell(window, x, y);
  cell->ch = (uint32_t) c;
  cell->fg = (uint16_t) fg;
  cell->bg = (uint16_t) bg;
}

void window_draw_cursor(struct window *window) {
  size_t cursor = window_cursor(window);
  struct gapbuf *gb = window->buffer->text;
  char c = gb_getchar(gb, cursor);
  size_t x, y;
  gb_pos_to_linecol(gb, cursor, &y, &x);

  window_change_cell(window,
      window_numberwidth(window) + x - window->left,
      y - window->top,
      c == '\n' ? ' ' : c,
      TB_BLACK,
      TB_WHITE);
}

void window_get_ruler(struct window *window, char *buf, size_t buflen) {
  struct gapbuf *gb = window->buffer->text;
  size_t line, col;
  gb_pos_to_linecol(gb, window_cursor(window), &line, &col);
  size_t nlines = gb_nlines(gb);
  size_t h = window_h(window);

  bool top = window->top == 0;
  bool bot = nlines < window->top + h + 1;
  char relpos[4];
  if (top && bot) {
    strcpy(relpos, "All");
  } else if (top) {
    strcpy(relpos, "Top");
  } else if (bot) {
    strcpy(relpos, "Bot");
  } else {
    size_t above = window->top - 1;
    size_t below = nlines - (window->top + h) + 1;
    size_t pct = (above * 100 / (above + below));
    snprintf(relpos, sizeof(relpos), "%zu%%", pct);
  }

  snprintf(buf, buflen, "%zu,%zu        %3s", line + 1, col + 1, relpos);
}

static void window_draw_plate(struct window *window) {
  size_t w = window_w(window);
  if (window_right(window)) {
    --w;
  }

  char name[256];
  snprintf(name, sizeof(name), "%s%s",
      *window->buffer->name ? window->buffer->name : "[No Name]",
      window->buffer->dirty ? " [+]" : "");

  char plate[256];

  if (option_get_bool("ruler")) {
    char ruler[32];
    window_get_ruler(window, ruler, sizeof(ruler));
    size_t namelen = strlen(name);
    snprintf(plate, sizeof(plate), "%-*s %*s",
        (int)namelen, name, (int)(w - namelen - 1), ruler);
  } else {
    strcpy(plate, name);
  }

  size_t platelen = strlen(plate);
  for (size_t x = 0; x < window_w(window); ++x) {
    char c = x < platelen ? plate[x] : ' ';
    window_change_cell(window, x, window_h(window) - 1, c, TB_BLACK, TB_WHITE);
  }
}

static void window_draw_line_number(struct window *window, size_t line) {
  bool number = option_get_bool("number");
  bool relativenumber = option_get_bool("relativenumber");

  if (!number && !relativenumber) {
    return;
  }

  size_t cursorline, cursorcol;
  gb_pos_to_linecol(window->buffer->text, window_cursor(window),
      &cursorline, &cursorcol);

  size_t linenumber = line + 1;
  if (relativenumber && !(number && line == cursorline)) {
    linenumber = (size_t) labs((ssize_t)(line - cursorline));
  }

  size_t col = window_numberwidth(window) - 2;
  do {
    size_t digit = linenumber % 10;
    window_change_cell(window, col--, line - window->top,
        (char) digit + '0', TB_YELLOW, TB_DEFAULT);
    linenumber = (linenumber - digit) / 10;
  } while (linenumber > 0);
}

static bool window_pos_is_visible(struct window *window, size_t pos) {
  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, pos, &line, &col);
  col += window_numberwidth(window);
  return window->top <= line && line < window->top + window_h(window) &&
         window->left <= col && col < window->left + window_w(window);
}

static void window_draw_visual_mode_selection(struct window *window) {
  if (!window->visual_mode_selection) {
    return;
  }

  for (size_t pos = window->visual_mode_selection->start;
      pos < window->visual_mode_selection->end; ++pos) {
    if (!window_pos_is_visible(window, pos)) {
      continue;
    }

    size_t line, col;
    gb_pos_to_linecol(window->buffer->text, pos, &line, &col);

    struct tb_cell *cell = window_get_cell(window,
        window_numberwidth(window) + col - window->left, line - window->top);
    cell->fg = TB_BLACK;
    cell->bg = TB_WHITE;
  }
}

static void window_draw_cursorline(struct window *window) {
  if (!option_get_bool("cursorline") || window->visual_mode_selection) {
    return;
  }

  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, window_cursor(window), &line, &col);

  size_t numberwidth = window_numberwidth(window);
  size_t width = window_w(window);
  for (size_t x = numberwidth; x < width; ++x) {
    window_get_cell(window, x, line - window->top)->fg |= TB_UNDERLINE;
  }
}

static void window_draw_leaf(struct window *window) {
  assert(window->split_type == WINDOW_LEAF);

  window_ensure_cursor_visible(window);
  struct gapbuf *gb = window->buffer->text;

  size_t w = window_w(window) - window_numberwidth(window);
  size_t h = window_h(window);

  size_t rows = min(gb->lines->len - window->top, h);
  size_t numberwidth = window_numberwidth(window);

  for (size_t y = 0; y < rows; ++y) {
    size_t line = y + window->top;
    window_draw_line_number(window, line);

    size_t cols = (size_t) max(0,
        min((ssize_t) gb->lines->buf[line] - (ssize_t) window->left, (ssize_t) w));
    for (size_t x = 0; x < cols; ++x) {
      size_t col = x + window->left;
      size_t pos = gb_linecol_to_pos(gb, line, col);
      char c = gb_getchar(gb, pos);
      window_change_cell(window, numberwidth + x, y, c, TB_WHITE, TB_DEFAULT);
    }
  }
  window_draw_visual_mode_selection(window);
  window_draw_cursorline(window);

  size_t nlines = gb_nlines(window->buffer->text);
  for (size_t y = nlines; y < h; ++y) {
    window_change_cell(window, 0, y, '~', TB_BLUE, TB_DEFAULT);
  }

  if (window_should_draw_plate(window)) {
    window_draw_plate(window);
  }
}

void window_draw(struct window *window) {
  if (window->split_type == WINDOW_LEAF) {
    window_draw_leaf(window);
    return;
  }

  window_draw(window->split.first);
  window_draw(window->split.second);
  if (window->split_type == WINDOW_SPLIT_VERTICAL) {
    struct window *left = window->split.first;
    for (size_t y = 0; y < window_h(left); ++y) {
      window_change_cell(left, window_w(left) - 1, y, '|', TB_BLACK, TB_WHITE);
    }
  }
}
