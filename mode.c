#include "mode.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "options.h"
#include "search.h"
#include "util.h"
#include "window.h"

static void digit_pressed(struct editor_t *editor, struct tb_event *ev) {
  if (!isdigit((int) ev->ch)) {
    editor_pop_mode(editor);
    editor_handle_key_press(editor, ev);
    return;
  }
  editor->count *= 10;
  editor->count += ev->ch - '0';
}

static struct editing_mode_t digit_mode_impl = {
  .entered = NULL,
  .exited = NULL,
  .key_pressed = digit_pressed,
  .parent = NULL
};

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

static struct editing_mode_t quote_mode_impl = {
  .entered = NULL,
  .exited = NULL,
  .key_pressed = quote_pressed,
  .parent = NULL
};

struct editing_mode_t *quote_mode(void) {
  return &quote_mode_impl;
}

struct cmdline_mode_t {
  struct editing_mode_t mode;
  // The position of the cursor when the mode was entered.
  size_t cursor;
  char prompt;
  // Called once the user presses enter, with the complete command.
  void (*done_cb)(struct editor_t*, char*);
  // Called after every character typed, with the command so far.
  void (*char_cb)(struct editor_t*, char*);
};

static void cmdline_mode_entered(struct editor_t *editor) {
  struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
  editor_status_msg(editor, "%c", mode->prompt);
  mode->cursor = editor->window->cursor;
  editor->status_silence = true;
}

static void cmdline_mode_exited(struct editor_t *editor) {
  struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
  editor->window->cursor = mode->cursor;
  editor->status_silence = false;
}

static void cmdline_mode_key_pressed(struct editor_t *editor, struct tb_event *ev) {
  char ch;
  struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
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
    } else if (mode->char_cb) {
      char *command = xstrdup(editor->status->buf + 1);
      mode->char_cb(editor, command);
      free(command);
    }
    return;
  case TB_KEY_ENTER: {
    char *command = xstrdup(editor->status->buf + 1);
    editor_pop_mode(editor);
    mode->done_cb(editor, command);
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
  if (mode->char_cb) {
    char *command = xstrdup(editor->status->buf + 1);
    mode->char_cb(editor, command);
    free(command);
  }
}

static void search_done_cb(struct editor_t *editor, char *command,
                           enum search_direction_t direction) {
  if (*command) {
    editor_search(editor, command, editor->window->cursor, direction);
    buf_printf(editor_get_register(editor, '/'), "%s", command);
  } else {
    editor_search(editor, NULL, editor->window->cursor, direction);
  }
}

static void search_char_cb(struct editor_t *editor, char *command,
                           enum search_direction_t direction) {
  if (!option_get_bool("incsearch") || !*command) {
    return;
  }
  struct cmdline_mode_t *mode = (struct cmdline_mode_t*) editor->mode;
  editor_search(editor, command, mode->cursor, direction);
}

static void forward_search_done_cb(struct editor_t *editor, char *command) {
  search_done_cb(editor, command, SEARCH_FORWARDS);
}

static void forward_search_char_cb(struct editor_t *editor, char *command) {
  search_char_cb(editor, command, SEARCH_FORWARDS);
}

static void backward_search_done_cb(struct editor_t *editor, char *command) {
  search_done_cb(editor, command, SEARCH_BACKWARDS);
}

static void backward_search_char_cb(struct editor_t *editor, char *command) {
  search_char_cb(editor, command, SEARCH_BACKWARDS);
}

#define CMDLINE_MODE_INIT \
  { \
    .entered = cmdline_mode_entered, \
    .exited = cmdline_mode_exited, \
    .key_pressed = cmdline_mode_key_pressed, \
    .parent = NULL \
  }


static struct cmdline_mode_t forward_search_impl = {
  CMDLINE_MODE_INIT,
  0, '/', forward_search_done_cb, forward_search_char_cb
};

static struct cmdline_mode_t backward_search_impl = {
  CMDLINE_MODE_INIT,
  0, '?', backward_search_done_cb, backward_search_char_cb
};

static struct cmdline_mode_t command_impl = {
  CMDLINE_MODE_INIT,
  0, ':', editor_execute_command, NULL
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
