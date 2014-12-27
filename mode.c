#include "mode.h"

#include <ctype.h>
#include <termbox.h>

static void no_op(editor_t *editor) {
}

static void digit_pressed(editor_t *editor, struct tb_event *ev) {
  if (!isdigit(ev->ch)) {
    editor_pop_mode(editor);
    editor_handle_key_press(editor, ev);
    return;
  }
  editor->count *= 10;
  editor->count += ev->ch - '0';
}

editing_mode_t digit_mode_impl = {
  no_op,
  digit_pressed
};

editing_mode_t *digit_mode(void) {
  return &digit_mode_impl;
}
