#include "mode.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "editor.h"
#include "motion.h"
#include "util.h"
#include "window.h"

struct visual_mode {
  struct editing_mode mode;
  enum visual_mode_kind kind;
  // The position of the cursor when the mode was entered.
  size_t cursor;
};

static void visual_mode_entered(struct editor *editor) {
  struct visual_mode *mode = (struct visual_mode*) editor->mode;
  // If we're entering the mode by popping (e.g. because we did a search and
  // finished), then keep the current anchor.
  if (!mode->cursor) {
    mode->cursor = window_cursor(editor->window);
    editor->window->visual_mode_selection = xmalloc(sizeof(struct region));
    visual_mode_selection_update(editor);
  }
  editor_status_msg(editor, "-- VISUAL --");
}

static void visual_mode_exited(struct editor *editor) {
  struct visual_mode *mode = (struct visual_mode*) editor->mode;
  mode->cursor = 0;
  free(editor->window->visual_mode_selection);
  editor->window->visual_mode_selection = NULL;
  buf_clear(editor->status);
}

static void visual_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  if (ev->ch != '0' && isdigit((int) ev->ch)) {
    editor->count = 0;
    while (isdigit((int) ev->ch)) {
      editor->count *= 10;
      editor->count += ev->ch - '0';
      editor_waitkey(editor, ev);
    }
  }

  if (ev->ch == '"') {
    editor_waitkey(editor, ev);
    char name = (char) tolower((int) ev->ch);
    if (editor_get_register(editor, name)) {
      editor->register_ = name;
    }
    return;
  }

  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    editor_pop_mode(editor);
    return;
  default: {
    struct motion *motion = motion_get(editor, ev);
    if (motion) {
      window_set_cursor(editor->window, motion_apply(motion, editor));
      return;
    }
    op_func *op = op_find((char) ev->ch);
    if (op) {
      struct region *selection = region_create(
          editor->window->visual_mode_selection->start,
          editor->window->visual_mode_selection->end);
      editor_pop_mode(editor);
      op(editor, selection);
      free(selection);
    }
  }
  }
}

static struct visual_mode impl = {
  {
    .entered = visual_mode_entered,
    .exited = visual_mode_exited,
    .key_pressed = visual_mode_key_pressed,
    .parent = NULL
  },
  VISUAL_MODE_CHARACTERWISE, 0
};

struct editing_mode *visual_mode(enum visual_mode_kind kind) {
  impl.kind = kind;
  return (struct editing_mode*) &impl;
}

// FIXME(ibadawi): Duplicated from motion.c
static bool is_line_start(struct gapbuf *gb, size_t pos) {
  return pos == 0 || gb_getchar(gb, pos - 1) == '\n';
}

static bool is_line_end(struct gapbuf *gb, size_t pos) {
  return pos == gb_size(gb) - 1 || gb_getchar(gb, pos) == '\n';
}

static size_t line_start(struct gapbuf *gb, size_t pos) {
  if (is_line_start(gb, pos)) {
    return pos;
  }
  return (size_t) (gb_lastindexof(gb, '\n', pos - 1) + 1);
}

static size_t line_end(struct gapbuf *gb, size_t pos) {
  if (is_line_end(gb, pos)) {
    return pos;
  }
  return gb_indexof(gb, '\n', pos);
}

void visual_mode_selection_update(struct editor *editor) {
  struct visual_mode *mode = (struct visual_mode*) editor->mode;
  while (mode && mode != &impl) {
    mode = (struct visual_mode*) mode->mode.parent;
  }
  if (!mode) {
    return;
  }

  struct region *selection = editor->window->visual_mode_selection;
  assert(selection);
  region_set(selection, window_cursor(editor->window), mode->cursor);

  struct gapbuf *gb = editor->window->buffer->text;
  if (mode->kind == VISUAL_MODE_LINEWISE) {
    selection->start = line_start(gb, selection->start);
    selection->end = line_end(gb, selection->end);
  }
  if (mode->kind == VISUAL_MODE_LINEWISE ||
      !is_line_end(gb, selection->end)) {
    ++selection->end;
  }
}
