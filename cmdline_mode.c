#include "mode.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <limits.h>
#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "history.h"
#include "search.h"
#include "tags.h"
#include "util.h"
#include "window.h"

void cmdline_mode_entered(struct editor *editor) {
  struct cmdline_mode *mode = editor_get_cmdline_mode(editor);
  mode->prompt = (char) mode->mode.arg;
  editor_status_msg(editor, "%c", mode->prompt);
  mode->cursor = window_cursor(editor->window);
  mode->history_entry = NULL;
  mode->history_prefix = buf_from_cstr("");

  mode->completions = NULL;
  mode->completion = NULL;

  editor->status_cursor = 1;
  editor->status_silence = true;
}

void cmdline_mode_exited(struct editor *editor) {
  struct cmdline_mode *mode = editor_get_cmdline_mode(editor);
  buf_free(mode->history_prefix);
  if (!editor->message->len) {
    editor->status_cursor = 0;
  }
  editor->status_silence = false;
  editor->window->have_incsearch_match = false;

  mode->completion = NULL;
  if (mode->completions) {
    history_deinit(mode->completions);
    free(mode->completions);
    mode->completions = NULL;
  }
}

static void search_done_cb(struct editor *editor, char *command) {
  editor->status_silence = false;
  struct cmdline_mode *mode = editor_get_cmdline_mode(editor);
  enum search_direction direction =
      mode->prompt == '/' ? SEARCH_FORWARDS : SEARCH_BACKWARDS;
  if (*command) {
    editor_jump_to_match(editor, command, mode->cursor, direction);
    struct editor_register *lsp = editor_get_register(editor, '/');
    lsp->write(lsp, command);
    history_add_item(&editor->search_history, command);
  } else {
    editor_jump_to_match(editor, NULL, mode->cursor, direction);
  }
  editor->highlight_search_matches = true;
}

EDITOR_COMMAND(nohlsearch, noh) {
  editor->highlight_search_matches = false;
}

static void search_char_cb(struct editor *editor, char *command) {
  if (!editor->opt.incsearch || !*command) {
    return;
  }
  struct cmdline_mode *mode = editor_get_cmdline_mode(editor);
  enum search_direction direction =
      mode->prompt == '/' ? SEARCH_FORWARDS : SEARCH_BACKWARDS;
  editor->window->have_incsearch_match = editor_search(
      editor, command, mode->cursor, direction, &editor->window->incsearch_match);
  if (editor->window->have_incsearch_match) {
    window_set_cursor(editor->window, editor->window->incsearch_match.start);
  } else {
    window_set_cursor(editor->window, mode->cursor);
  }
}

static void command_done_cb(struct editor *editor, char *command) {
  editor->status_silence = false;
  history_add_item(&editor->command_history, command);
  editor_execute_command(editor, command);
}

void editor_load_completions(struct editor *editor,
    enum completion_kind kind, struct history *history) {
  switch (kind) {
  case COMPLETION_NONE: assert(0); break;
  case COMPLETION_CMDS: {
    int len;
    char **completions = commands_get_sorted(&len);
    for (int i = len - 1; i >= 0; --i) {
      history_add_item(history, completions[i]);
    }
    free(completions);
    break;
  }
  case COMPLETION_OPTIONS: {
    int len;
    char **options = options_get_sorted(&len);
    for (int i = len - 1; i >= 0; --i) {
      history_add_item(history, options[i]);
    }
    free(options);
    break;
  }
  case COMPLETION_TAGS:
    for (size_t i = 0; i < editor->tags->len; ++i) {
      size_t j = editor->tags->len - 1 - i;
      history_add_item(history, editor->tags->tags[j].name);
    }
    break;
  case COMPLETION_PATHS: {
    // TODO(ibadawi): completion for subdirectories
    char buf[PATH_MAX];
    struct dirent **namelist;
    int n;

    n = scandir(".", &namelist, NULL, alphasort);
    if (n <= 0) {
      break;
    }

    for (int i = n - 1; i >= 0; --i) {
      struct dirent *entry = namelist[i];
      if (*entry->d_name == '.') {
        free(entry);
        continue;
      }

      strcpy(buf, entry->d_name);
      if (entry->d_type == DT_DIR) {
        strcat(buf, "/");
      }
      history_add_item(history, buf);
      free(entry);
    }
    free(namelist);
    break;
  }
  }
}

void cmdline_mode_key_pressed(struct editor *editor, struct tb_event *ev) {
  char ch;
  struct cmdline_mode *mode = editor_get_cmdline_mode(editor);

  void (*done_cb)(struct editor*, char*);
  void (*char_cb)(struct editor*, char*);
  struct history *history;
  switch (mode->prompt) {
    case ':':
      history = &editor->command_history;
      done_cb = command_done_cb;
      char_cb = NULL;
      break;
    case '/': case '?':
      history = &editor->search_history;
      done_cb = search_done_cb;
      char_cb = search_char_cb;
      break;
    default: assert(0);
  }

  switch (ev->key) {
  case TB_KEY_TAB: {
    if (mode->prompt != ':') {
      return;
    }

    char *command = mode->history_prefix->buf;
    enum completion_kind kind = COMPLETION_NONE;
    char *arg = NULL;
    struct editor_command *cmd = command_parse(command, &arg, NULL);
    size_t skip = 0;
    if (cmd && arg && cmd->completion_kind != COMPLETION_NONE) {
      kind = cmd->completion_kind;
      skip = arg - command;
    } else if (!strchr(command, ' ')) {
      kind = COMPLETION_CMDS;
    }

    if (kind == COMPLETION_NONE) {
      return;
    }

    if (mode->completions && mode->completion_kind != kind) {
      history_deinit(mode->completions);
      free(mode->completions);
      mode->completions = NULL;
      mode->completion = NULL;
    }
    mode->completion_kind = kind;

    if (!mode->completions) {
      mode->completions = xmalloc(sizeof(*mode->completions));
      history_init(mode->completions, NULL);
      editor_load_completions(editor, kind, mode->completions);
    }

    if (mode->completion) {
      mode->completion = history_next(mode->completion,
          buf_startswith, command + skip);
    }
    if (!mode->completion) {
      mode->completion = history_first(mode->completions,
          buf_startswith, command + skip);
    }

    if (mode->completion) {
      char c = command[skip];
      command[skip] = '\0';
      buf_printf(editor->status, "%c%s%s",
          mode->prompt, command, mode->completion->buf->buf);
      command[skip] = c;
      editor->status_cursor = editor->status->len;
    }
    return;
  }
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    buf_clear(editor->status);
    window_set_cursor(editor->window, mode->cursor);
    editor_pop_mode(editor);
    return;
  case TB_KEY_ARROW_LEFT:
    if (editor->status_cursor == 1) {
      return;
    }
    editor->status_cursor--;
    if (ev->meta == TB_META_SHIFT) {
      while (editor->status_cursor > 1 &&
          !isspace(editor->status->buf[editor->status_cursor - 1])) {
        editor->status_cursor--;
      }
    }
    return;
  case TB_KEY_ARROW_RIGHT:
    if (editor->status_cursor == editor->status->len) {
      return;
    }
    editor->status_cursor++;
    if (ev->meta == TB_META_SHIFT) {
      while (editor->status_cursor < editor->status->len &&
          !isspace(editor->status->buf[editor->status_cursor])) {
        editor->status_cursor++;
      }
    }
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
  case TB_KEY_BACKSPACE:
    buf_delete(editor->status, --editor->status_cursor, 1);
    buf_printf(mode->history_prefix, "%s", editor->status->buf + 1);
    if (editor->status->len == 0) {
      window_set_cursor(editor->window, mode->cursor);
      editor_pop_mode(editor);
      return;
    } else if (char_cb) {
      char *command = xstrdup(editor->status->buf + 1);
      char_cb(editor, command);
      free(command);
    }
    return;
  case TB_KEY_ENTER: {
    char *command = xstrdup(editor->status->buf + 1);
    done_cb(editor, command);
    editor_pop_mode(editor);
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
  if (char_cb) {
    char *command = xstrdup(editor->status->buf + 1);
    char_cb(editor, command);
    free(command);
  }
}
