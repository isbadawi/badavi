#include "mode.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "history.h"
#include "search.h"
#include "util.h"
#include "window.h"

struct cmdline_mode {
  struct editing_mode mode;
  // The position of the cursor when the mode was entered.
  size_t cursor;
  struct history_entry *history_entry;
  struct buf *history_prefix;
  char prompt;
  // Called once the user presses enter, with the complete command.
  void (*done_cb)(struct editor*, char*);
  // Called after every character typed, with the command so far.
  void (*char_cb)(struct editor*, char*);
};

static void cmdline_mode_entered(struct editor *editor) {
  struct cmdline_mode *mode = (struct cmdline_mode*) editor->mode;
  editor_status_msg(editor, "%c", mode->prompt);
  mode->cursor = window_cursor(editor->window);
  mode->history_entry = NULL;
  mode->history_prefix = buf_from_cstr("");
  editor->status_cursor = 1;
  editor->status_silence = true;
}

static void clear_incsearch_match(struct editor *editor) {
  struct search_match **match = &editor->window->incsearch_match;
  if (*match) {
    free(*match);
    *match = NULL;
  }
}

static void cmdline_mode_exited(struct editor *editor) {
  struct cmdline_mode *mode = (struct cmdline_mode*) editor->mode;
  window_set_cursor(editor->window, mode->cursor);
  buf_free(mode->history_prefix);
  editor->status_cursor = 0;
  editor->status_silence = false;
  clear_incsearch_match(editor);
}

static void cmdline_mode_key_pressed(struct editor *editor, struct tb_event *ev) {
  char ch;
  struct cmdline_mode *mode = (struct cmdline_mode*) editor->mode;

  struct history *history;
  switch (mode->prompt) {
    case ':': history = &editor->command_history; break;
    case '/': case '?': history = &editor->search_history; break;
    default: assert(0);
  }

  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    buf_clear(editor->status);
    editor_pop_mode(editor);
    return;
  // FIXME(ibadawi): termbox doesn't support shift + arrow keys.
  // vim uses <S-Left>, <S-Right> for moving cursor to prev/next WORD.
  case TB_KEY_ARROW_LEFT:
    editor->status_cursor = max(editor->status_cursor - 1, 1);
    return;
  case TB_KEY_ARROW_RIGHT:
    editor->status_cursor = min(editor->status_cursor + 1, editor->status->len);
    return;
  case TB_KEY_ARROW_UP:
    if (!mode->history_entry) {
      mode->history_entry = history_first(
          history, buf_startswith, mode->history_prefix->buf);
    } else {
      struct history_entry *next = history_next(
          mode->history_entry, buf_startswith, mode->history_prefix->buf);
      if (!next) {
        return;
      }
      mode->history_entry = next;
    }
    if (mode->history_entry) {
      struct buf *command = mode->history_entry->buf;
      buf_printf(editor->status, "%c%s", mode->prompt, command->buf);
      editor->status_cursor = command->len + 1;
    }
    return;
  case TB_KEY_ARROW_DOWN:
    if (mode->history_entry) {
      mode->history_entry = history_prev(
          mode->history_entry, buf_startswith, mode->history_prefix->buf);
    }
    if (mode->history_entry) {
      struct buf *command = mode->history_entry->buf;
      buf_printf(editor->status, "%c%s", mode->prompt, command->buf);
      editor->status_cursor = command->len + 1;
    } else {
      buf_printf(editor->status, "%c%s", mode->prompt, mode->history_prefix->buf);
      editor->status_cursor = mode->history_prefix->len + 1;
    }
    return;
  case TB_KEY_CTRL_B: case TB_KEY_HOME:
    editor->status_cursor = 1;
    return;
  case TB_KEY_CTRL_E: case TB_KEY_END:
    editor->status_cursor = editor->status->len;
    return;
  case TB_KEY_BACKSPACE2:
    buf_delete(editor->status, --editor->status_cursor, 1);
    buf_printf(mode->history_prefix, "%s", editor->status->buf + 1);
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
  buf_insert(editor->status, s, editor->status_cursor++);
  buf_printf(mode->history_prefix, "%s", editor->status->buf + 1);
  if (mode->char_cb) {
    char *command = xstrdup(editor->status->buf + 1);
    mode->char_cb(editor, command);
    free(command);
  }
}

static void search_done_cb(struct editor *editor, char *command,
                           enum search_direction direction) {
  size_t cursor = window_cursor(editor->window);
  if (*command) {
    editor_jump_to_match(editor, command, cursor, direction);
    struct editor_register *lsp = editor_get_register(editor, '/');
    lsp->write(lsp, command);
    history_add_item(&editor->search_history, command);
  } else {
    editor_jump_to_match(editor, NULL, cursor, direction);
  }
  editor->highlight_search_matches = true;
}

static void search_char_cb(struct editor *editor, char *command,
                           enum search_direction direction) {
  if (!editor->opt.incsearch || !*command) {
    return;
  }
  struct cmdline_mode *mode = (struct cmdline_mode*) editor->mode;
  struct search_match *match =
    editor_search(editor, command, mode->cursor, direction);
  if (match) {
    editor->window->incsearch_match = match;
    window_set_cursor(editor->window, match->region.start);
  } else {
    free(match);
    window_set_cursor(editor->window, mode->cursor);
    clear_incsearch_match(editor);
  }
}

static void forward_search_done_cb(struct editor *editor, char *command) {
  search_done_cb(editor, command, SEARCH_FORWARDS);
}

static void forward_search_char_cb(struct editor *editor, char *command) {
  search_char_cb(editor, command, SEARCH_FORWARDS);
}

static void backward_search_done_cb(struct editor *editor, char *command) {
  search_done_cb(editor, command, SEARCH_BACKWARDS);
}

static void backward_search_char_cb(struct editor *editor, char *command) {
  search_char_cb(editor, command, SEARCH_BACKWARDS);
}

static void command_done_cb(struct editor *editor, char *command) {
  history_add_item(&editor->command_history, command);
  editor_execute_command(editor, command);
}

#define CMDLINE_MODE_INIT \
  { \
    .entered = cmdline_mode_entered, \
    .exited = cmdline_mode_exited, \
    .key_pressed = cmdline_mode_key_pressed, \
    .parent = NULL \
  }, 0, NULL, NULL


static struct cmdline_mode forward_search_impl = {
  CMDLINE_MODE_INIT,
  '/', forward_search_done_cb, forward_search_char_cb
};

static struct cmdline_mode backward_search_impl = {
  CMDLINE_MODE_INIT,
  '?', backward_search_done_cb, backward_search_char_cb
};

static struct cmdline_mode command_impl = {
  CMDLINE_MODE_INIT,
  ':', command_done_cb, NULL
};

struct editing_mode *search_mode(char direction) {
  if (direction == '/') {
    return (struct editing_mode*) &forward_search_impl;
  } else {
    return (struct editing_mode*) &backward_search_impl;
  }
}

struct editing_mode *command_mode(void) {
  return (struct editing_mode*) &command_impl;
}
