#include "mode.h"

#include <ctype.h>
#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "motion.h"
#include "util.h"
#include "window.h"

static void visual_mode_entered(struct editor_t *editor) {
  size_t cursor = window_cursor(editor->window);
  editor->window->visual_mode_anchor = region_create(cursor, cursor + 1);
  editor_status_msg(editor, "-- VISUAL --");
}

static void visual_mode_exited(struct editor_t *editor) {
  free(editor->window->visual_mode_anchor);
  editor->window->visual_mode_anchor = NULL;
  buf_clear(editor->status);
}

static void visual_mode_key_pressed(struct editor_t* editor, struct tb_event* ev) {
  if (ev->ch != '0' && isdigit((int) ev->ch)) {
    editor->count = 0;
    while (isdigit((int) ev->ch)) {
      editor->count *= 10;
      editor->count += ev->ch - '0';
      editor_waitkey(editor, ev);
    }
  }

  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    editor_pop_mode(editor);
    return;
  default: {
    struct motion_t *motion = motion_get(editor, ev);
    if (motion) {
      window_set_cursor(editor->window, motion_apply(motion, editor));
      return;
    }
    op_t *op = op_find((char) ev->ch);
    if (op) {
      struct region_t selection;
      region_set(
          &selection,
          editor->window->cursor->start,
          editor->window->visual_mode_anchor->start);
      ++selection.end;
      op(editor, &selection);
    }
  }
  }
}

static struct editing_mode_t impl = {
  .entered = visual_mode_entered,
  .exited = visual_mode_exited,
  .key_pressed = visual_mode_key_pressed,
  .parent = NULL
};

struct editing_mode_t *visual_mode(void) {
  return &impl;
}
