#include "window.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "options.h"
#include "tags.h"
#include "util.h"

struct window *window_create(struct buffer *buffer, size_t w, size_t h) {
  struct window *window = xmalloc(sizeof(*window));

  window->split_type = WINDOW_LEAF;
  window->parent = NULL;
  window->pwd = NULL;

  window->buffer = NULL;
  window->alternate_path = NULL;
  window->cursor = xmalloc(sizeof(*window->cursor));
  if (buffer) {
    window_set_buffer(window, buffer);
  }
  window->visual_mode_selection = NULL;

  window->w = w;
  window->h = h;

  TAILQ_INIT(&window->tag_stack);
  window->tag = NULL;

  window_init_options(window);

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
  window_inherit_parent_options(copy);

  sibling->parent = window;
  window_inherit_parent_options(sibling);

  if (window->pwd) {
    copy->pwd = window->pwd;
    sibling->pwd = xstrdup(window->pwd);
    window->pwd = NULL;
  }

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

  return sibling;
}

// FIXME(ibadawi): Enforce minimum size for a window
void window_resize(struct window *window, int dw, int dh) {
  if (!window->parent) {
    return;
  }
  assert((dw && !dh) || (!dw && dh));

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
    TAILQ_REMOVE(&window->buffer->marks, window->cursor, pointers);

    if (window->buffer->path) {
      free(window->alternate_path);
      window->alternate_path = xstrdup(window->buffer->path);
    }
  }

  window->buffer = buffer;
  window->top = 0;
  window->left = 0;
  region_set(&window->cursor->region, 0, 1);
  window->have_incsearch_match = false;
  TAILQ_INSERT_TAIL(&buffer->marks, window->cursor, pointers);
}

size_t window_cursor(struct window *window) {
  return window->cursor->region.start;
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

void window_free(struct window *window) {
  if (window->split_type == WINDOW_LEAF) {
    struct tag_jump *j, *tj;
    TAILQ_FOREACH_SAFE(j, &window->tag_stack, pointers, tj) {
      free(j);
    }
    if (window->buffer) {
      TAILQ_REMOVE(&window->buffer->marks, window->cursor, pointers);
      free(window->cursor);
    }
    free(window->pwd);
    free(window->alternate_path);
  } else {
    window_free(window->split.first);
    window_free(window->split.second);
  }
  window_free_options(window);
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

  // free, not window_free, because that would free parent's fields.
  free(sibling);

  return window_first_leaf(parent);
}

void window_set_cursor(struct window *window, size_t pos) {
  region_set(&window->cursor->region, pos, pos + 1);
}

void window_center_cursor(struct window *window) {
  size_t cursor = window_cursor(window);
  size_t line, col;
  gb_pos_to_linecol(window->buffer->text, cursor, &line, &col);

  size_t h = window_h(window);
  if (line > h/2) {
    window->top = line - h/2;
  }
}

void window_page_up(struct window *window) {
  if (window->top == 0) {
    return;
  }

  window_set_cursor(window, gb_linecol_to_pos(
      window->buffer->text, window->top + 1, 0));

  if (window->top < window_h(window)) {
    window->top = 0;
  } else {
    window->top -= window_h(window);
  }
}

void window_page_down(struct window *window) {
  size_t nlines = gb_nlines(window->buffer->text);
  if (window->top == nlines - 1) {
    return;
  }

  if (window->top + window_h(window) > nlines - 3) {
    window->top = nlines - 1;
  } else {
    window->top += window_h(window) - 2;
  }

  window_set_cursor(window, gb_linecol_to_pos(
      window->buffer->text, window->top, 0));
}

EDITOR_COMMAND_WITH_COMPLETION(split, sp, COMPLETION_PATHS) {
  editor_set_window(editor, window_split(editor->window,
      editor->opt.splitbelow ? WINDOW_SPLIT_BELOW : WINDOW_SPLIT_ABOVE));

  if (editor->opt.equalalways) {
    window_equalize(editor->window, WINDOW_SPLIT_HORIZONTAL);
  }

  if (arg) {
    editor_open(editor, arg);
  }
}

EDITOR_COMMAND(splitfind, sfind) {
  if (!arg) {
    editor_status_err(editor, "No file name");
    return;
  }

  char *path = editor_find_in_path(editor, arg);
  if (!path) {
    editor_status_err(editor, "Can't find file \"%s\" in path", arg);
    return;
  }

  editor_command_split(editor, path, false);
  free(path);
}

EDITOR_COMMAND_WITH_COMPLETION(vsplit, vsp, COMPLETION_PATHS) {
  editor_set_window(editor, window_split(editor->window,
      editor->opt.splitright ? WINDOW_SPLIT_RIGHT : WINDOW_SPLIT_LEFT));

  if (editor->opt.equalalways) {
    window_equalize(editor->window, WINDOW_SPLIT_VERTICAL);
  }

  if (arg) {
    editor_open(editor, arg);
  }
}

void window_clear_working_directories(struct window *window) {
  if (window->split_type == WINDOW_LEAF) {
    free(window->pwd);
    window->pwd = NULL;
    return;
  }

  window_clear_working_directories(window->split.first);
  window_clear_working_directories(window->split.second);
}
