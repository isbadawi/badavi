#include "mode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "editor.h"

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

static void quote_pressed(editor_t *editor, struct tb_event *ev) {
  char name = tolower(ev->ch);
  if (editor_get_register(editor, name)) {
    editor->register_ = name;
  }
  editor_pop_mode(editor);
}

editing_mode_t quote_mode_impl = {
  no_op,
  quote_pressed
};

editing_mode_t *quote_mode(void) {
  return &quote_mode_impl;
}

typedef void (cmdline_mode_func_t) (editor_t*, char*);

typedef struct {
  editing_mode_t mode;
  char prompt;
  cmdline_mode_func_t *cb;
} cmdline_mode_t;


static void cmdline_mode_entered(editor_t *editor) {
  cmdline_mode_t *mode = (cmdline_mode_t*) editor->mode;
  editor_status_msg(editor, "%c", mode->prompt);
}

static void cmdline_mode_key_pressed(editor_t *editor, struct tb_event *ev) {
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    buf_clear(editor->status);
    editor_pop_mode(editor);
    return;
  case TB_KEY_BACKSPACE2:
    buf_delete(editor->status, editor->status->len - 1, 1);
    if (editor->status->len == 0) {
      editor_pop_mode(editor);
      return;
    }
    return;
  case TB_KEY_ENTER: {
    cmdline_mode_t *mode = (cmdline_mode_t*) editor->mode;
    char *command = strndup(editor->status->buf + 1, editor->status->len - 1);
    mode->cb(editor, command);
    free(command);
    editor_pop_mode(editor);
    return;
  }
  case TB_KEY_SPACE:
    ch = ' ';
    break;
  default:
    ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_append(editor->status, s);
}

static void search_mode_cmdline_cb(editor_t *editor, char *command) {
  if (*command) {
    buf_t *reg = editor_get_register(editor, '/');
    buf_clear(reg);
    buf_append(reg, command);
  }
  // Search anyway -- empty pattern means repeat the last search.
  editor_search(editor);
}

cmdline_mode_t search_impl = {
  {cmdline_mode_entered, cmdline_mode_key_pressed},
  '/', search_mode_cmdline_cb
};

cmdline_mode_t command_impl = {
  {cmdline_mode_entered, cmdline_mode_key_pressed},
  ':', editor_execute_command
};

editing_mode_t *search_mode(void) {
  return (editing_mode_t*) &search_impl;
}

editing_mode_t *command_mode(void) {
  return (editing_mode_t*) &command_impl;
}
