#include "mode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"

static void digit_pressed(struct editor_t *editor, struct tb_event *ev) {
  if (!isdigit((int) ev->ch)) {
    editor_pop_mode(editor);
    editor_handle_key_press(editor, ev);
    return;
  }
  editor->count *= 10;
  editor->count += ev->ch - '0';
}

static struct editing_mode_t digit_mode_impl = {NULL, digit_pressed, NULL};

struct editing_mode_t *digit_mode(void) {
  return &digit_mode_impl;
}

static void quote_pressed(struct editor_t *editor, struct tb_event *ev) {
  char name = (char) tolower((int) ev->ch);
  if (editor_get_register(editor, name)) {
    editor->register_ = name;
  }
  editor_pop_mode(editor);
}

static struct editing_mode_t quote_mode_impl = {NULL, quote_pressed, NULL};

struct editing_mode_t *quote_mode(void) {
  return &quote_mode_impl;
}

struct cmdline_mode_t {
  struct editing_mode_t mode;
  char prompt;
  void (*cb)(struct editor_t*, char*);
};

static void cmdline_mode_entered(struct editor_t *editor) {
  struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
  editor_status_msg(editor, "%c", mode->prompt);
}

static void cmdline_mode_key_pressed(struct editor_t *editor, struct tb_event *ev) {
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
    struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
    char *command = strndup(editor->status->buf + 1, editor->status->len - 1);
    editor_pop_mode(editor);
    mode->cb(editor, command);
    free(command);
    return;
  }
  case TB_KEY_SPACE:
    ch = ' ';
    break;
  default:
    ch = (char) ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_append(editor->status, s);
}

static void forward_search_mode_cmdline_cb(struct editor_t *editor, char *command) {
  if (*command) {
    struct buf_t *reg = editor_get_register(editor, '/');
    buf_clear(reg);
    buf_append(reg, command);
  }
  editor_search(editor, EDITOR_SEARCH_FORWARDS);
}

static void backward_search_mode_cmdline_cb(struct editor_t *editor, char *command) {
  if (*command) {
    struct buf_t *reg = editor_get_register(editor, '/');
    buf_clear(reg);
    buf_append(reg, command);
  }
  editor_search(editor, EDITOR_SEARCH_BACKWARDS);
}


static struct cmdline_mode_t forward_search_impl = {
  {cmdline_mode_entered, cmdline_mode_key_pressed, NULL},
  '/', forward_search_mode_cmdline_cb
};

static struct cmdline_mode_t backward_search_impl = {
  {cmdline_mode_entered, cmdline_mode_key_pressed, NULL},
  '?', backward_search_mode_cmdline_cb
};

static struct cmdline_mode_t command_impl = {
  {cmdline_mode_entered, cmdline_mode_key_pressed, NULL},
  ':', editor_execute_command
};

struct editing_mode_t *search_mode(char direction) {
  if (direction == '/') {
    return (struct editing_mode_t*) &forward_search_impl;
  } else {
    return (struct editing_mode_t*) &backward_search_impl;
  }
}

struct editing_mode_t *command_mode(void) {
  return (struct editing_mode_t*) &command_impl;
}
