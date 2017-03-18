#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <regex.h>
#include <unistd.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "list.h"
#include "mode.h"
#include "options.h"
#include "tags.h"
#include "terminal.h"
#include "util.h"
#include "window.h"

#define R(n) {n, NULL}
static struct editor_register register_table[] = {
  // Last search pattern register
  R('/'),
  // Unnamed default register
  R('"'),
  // Named registers
  R('a'), R('b'), R('c'), R('d'), R('e'), R('f'), R('g'), R('h'), R('i'), R('j'), R('k'),
  R('l'), R('m'), R('n'), R('o'), R('p'), R('q'), R('r'), R('s'), R('t'), R('u'), R('v'),
  R('w'), R('x'), R('y'), R('z'),
  // Sentinel
  R(-1)
};
#undef R

void editor_init(struct editor *editor, size_t width, size_t height) {
  editor->buffers = list_create();
  struct buffer *buffer = buffer_create(NULL);
  list_append(editor->buffers, buffer);

  editor->width = width;
  editor->height = height;
  editor->window = window_create(buffer, editor->width, editor->height - 1);

  editor->status = buf_create(editor->width);
  editor->status_error = false;
  editor->status_silence = false;
  editor->status_cursor = 0;

  editor->mode = normal_mode();

  editor->registers = register_table;
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    editor->registers[i].buf = buf_create(1);
  }

  editor->tags = tags_create("tags");

  editor->count = 0;
  editor->register_ = '"';

  editor->synthetic_events = list_create();
}

struct buf *editor_get_register(struct editor *editor, char name) {
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    if (editor->registers[i].name == name) {
      return editor->registers[i].buf;
    }
  }
  return NULL;
}

static struct buffer *editor_get_buffer_by_name(struct editor* editor, char *name) {
  struct buffer *b;
  LIST_FOREACH(editor->buffers, b) {
    if (!strcmp(b->name, name)) {
      return b;
    }
  }
  return NULL;
}

void editor_open(struct editor *editor, char *path) {
  struct buffer *buffer = editor_get_buffer_by_name(editor, path);

  if (!buffer) {
    if (access(path, F_OK) < 0) {
      buffer = buffer_create(path);
      editor_status_msg(editor, "\"%s\" [New File]", path);
    } else {
      buffer = buffer_open(path);
      editor_status_msg(editor, "\"%s\" %zuL, %zuC",
          path, gb_nlines(buffer->text), gb_size(buffer->text));
    }
    list_append(editor->buffers, buffer);
  }
  window_set_buffer(editor->window, buffer);
}

void editor_push_mode(struct editor *editor, struct editing_mode *mode) {
  mode->parent = editor->mode;
  editor->mode = mode;
  if (editor->mode->entered) {
    editor->mode->entered(editor);
  }
}

void editor_pop_mode(struct editor *editor) {
  if (editor->mode->exited) {
    editor->mode->exited(editor);
  }
  if (editor->mode->parent) {
    editor->mode = editor->mode->parent;
    if (editor->mode->entered) {
      editor->mode->entered(editor);
    }
  }
}

void editor_save_buffer(struct editor *editor, char *path) {
  struct buffer *buffer = editor->window->buffer;
  char *name;
  bool rc;
  if (path) {
    rc = buffer_saveas(buffer, path);
    name = path;
  } else {
    rc = buffer_write(buffer);
    name = buffer->name;
  }
  if (rc) {
    editor_status_msg(editor, "\"%s\" %zuL, %zuC written",
        name, gb_nlines(buffer->text), gb_size(buffer->text));
  } else {
    editor_status_err(editor, "No file name");
  }
}

struct editor_command {
  const char *name;
  const char *shortname;
  void (*action)(struct editor*, char*);
};

static void editor_command_quit(struct editor *editor, char __unused *arg) {
  struct buffer *b;
  LIST_FOREACH(editor->buffers, b) {
    if (b->dirty) {
      editor_status_err(editor,
          "No write since last change for buffer \"%s\"",
          *b->name ? b->name : "[No Name]");
      return;
    }
  }
  exit(0);
}

__attribute__((noreturn))
static void editor_command_force_quit(struct editor __unused *editor,
                                      char __unused *arg) {
  exit(0);
}

static void editor_command_force_close_window(struct editor *editor, char *arg) {
  if (window_root(editor->window)->split_type == WINDOW_LEAF) {
    editor_command_force_quit(editor, arg);
  }
  assert(editor->window->parent);

  editor->window = window_close(editor->window);
}

static void editor_command_close_window(struct editor *editor, char *arg) {
  if (window_root(editor->window)->split_type == WINDOW_LEAF) {
    editor_command_quit(editor, arg);
    return;
  }

  if (editor->window->buffer->dirty) {
      editor_status_err(editor,
          "No write since last change (add ! to override)");
      return;
  }

  editor_command_force_close_window(editor, arg);
}

static void editor_command_write_quit(struct editor *editor, char *arg) {
  editor_save_buffer(editor, NULL);
  editor_command_close_window(editor, arg);
}

static void editor_command_edit(struct editor *editor, char *arg) {
  if (arg) {
    editor_open(editor, arg);
  } else {
    editor_status_err(editor, "No file name");
  }
}

void editor_source(struct editor *editor, char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    editor_status_err(editor, "Can't open file %s", path);
    return;
  }

  size_t n = 0;
  char *line = NULL;
  ssize_t len = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    line[len - 1] = '\0';
    editor_execute_command(editor, line);
  }
  free(line);

  fclose(fp);
}

static void editor_command_source(struct editor *editor, char *arg) {
  if (!arg) {
    editor_status_err(editor, "Argument required");
    return;
  }

  editor_source(editor, arg);
}

static void editor_command_set(struct editor *editor, char *arg) {
  if (!arg) {
    // TODO(isbadawi): show current values of all options...
    editor_status_err(editor, "Argument required");
    return;
  }

  regex_t regex;
  regcomp(&regex, "(no)?([a-z]+)(=[0-9]+|!|\\?)?", REG_EXTENDED);

  regmatch_t groups[4];
  int nomatch = regexec(&regex, arg, 4, groups, 0);
  regfree(&regex);

  if (nomatch) {
    editor_status_err(editor, "Invalid argument: %s", arg);
    return;
  }

  char opt[32];
  size_t optlen = (size_t) (groups[2].rm_eo - groups[2].rm_so);
  strncpy(opt, arg + groups[2].rm_so, optlen);
  opt[optlen] = '\0';

  if (!option_exists(opt)) {
    editor_status_err(editor, "Unknown option: %s", opt);
    return;
  }

  if (groups[3].rm_so == -1) {
    if (option_is_int(opt)) {
      editor_status_msg(editor, "%s=%d", opt, option_get_int(opt));
    } else if (option_is_bool(opt)) {
      option_set_bool(opt, groups[1].rm_so == -1);
    }
    return;
  }

  switch (arg[groups[3].rm_so]) {
  case '!':
    if (option_is_bool(opt)) {
      option_set_bool(opt, !option_get_bool(opt));
    } else {
      editor_status_err(editor, "Invalid argument: %s", arg);
    }
    break;
  case '?':
    if (option_is_int(opt)) {
      editor_status_msg(editor, "%s=%d", opt, option_get_int(opt));
    } else {
      editor_status_msg(editor, "%s%s", option_get_bool(opt) ? "" : "no", opt);
    }
    break;
  case '=':
    if (option_is_int(opt)) {
      option_set_int(opt, atoi(arg + groups[3].rm_so + 1));
    } else {
      editor_status_err(editor, "Invalid argument: %s", arg);
    }
    break;
  }
  return;
}

static void editor_command_split(struct editor *editor, char *arg) {
  editor->window = window_split(editor->window, WINDOW_SPLIT_HORIZONTAL);

  if (arg) {
    editor_command_edit(editor, arg);
  }
}

static void editor_command_vsplit(struct editor *editor, char *arg) {
  editor->window = window_split(editor->window, WINDOW_SPLIT_VERTICAL);

  if (arg) {
    editor_command_edit(editor, arg);
  }
}

static void editor_command_tag(struct editor *editor, char *arg) {
  if (!arg) {
    editor_tag_stack_next(editor);
  } else {
    editor_jump_to_tag(editor, arg);
  }
}

static struct editor_command editor_commands[] = {
  {"quit", "q", editor_command_close_window},
  {"quit!", "q!", editor_command_force_close_window},
  {"qall", "qa", editor_command_quit},
  {"qall!", "qa!", editor_command_force_quit},
  {"write", "w", editor_save_buffer},
  {"wq", "wq", editor_command_write_quit},
  {"edit", "e", editor_command_edit},
  {"source", "so", editor_command_source},
  {"set", "set", editor_command_set},
  {"split", "sp", editor_command_split},
  {"vsplit", "vsp", editor_command_vsplit},
  {"tag", "tag", editor_command_tag},
  {NULL, NULL, NULL}
};

void editor_execute_command(struct editor *editor, char *command) {
  if (!*command) {
    return;
  }
  char *name = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  for (int i = 0; editor_commands[i].name; ++i) {
    struct editor_command *cmd = &editor_commands[i];
    if (!strcmp(name, cmd->name) || !strcmp(name, cmd->shortname)) {
      cmd->action(editor, arg);
      return;
    }
  }

  int line = atoi(name);
  char buf[32];
  if (line > 0) {
    snprintf(buf, 32, "%dG", line);
    editor_send_keys(editor, buf);
  } else if (line < 0) {
    snprintf(buf, 32, "%dk", line);
    editor_send_keys(editor, buf);
  } else {
    editor_status_err(editor, "Not an editor command: %s", command);
  }
}

void editor_draw(struct editor *editor) {
  tb_clear();

  window_draw(window_root(editor->window));
  window_draw_cursor(editor->window);

  for (size_t x = 0; x < editor->status->len; ++x) {
    tb_change_cell((int) x, (int) editor->height - 1, (uint32_t) editor->status->buf[x],
        editor->status_error ? TB_DEFAULT : TB_WHITE,
        editor->status_error ? TB_RED : TB_DEFAULT);
  }

  if (editor->status_cursor) {
    tb_change_cell((int) editor->status_cursor, (int) editor->height - 1,
        (uint32_t) editor->status->buf[editor->status_cursor],
        TB_BLACK, TB_WHITE);
  }

  tb_present();
}

static void editor_suspend(struct editor *editor) {
  terminal_suspend();
  editor_draw(editor);
}

static int editor_poll_event(struct editor *editor, struct tb_event *ev) {
  if (!list_empty(editor->synthetic_events)) {
    struct tb_event *top = list_pop(editor->synthetic_events);
    *ev = *top;
    free(top);
    return ev->type;
  }
  return tb_poll_event(ev);
}

bool editor_waitkey(struct editor *editor, struct tb_event *ev) {
  if (editor_poll_event(editor, ev) < 0) {
    return false;
  }
  if (ev->type == TB_EVENT_KEY && ev->key == TB_KEY_CTRL_Z) {
    editor_suspend(editor);
  } else if (ev->type == TB_EVENT_RESIZE) {
    editor->width = (size_t) ev->w;
    editor->height = (size_t) ev->h;
    struct window *root = window_root(editor->window);
    root->w = editor->width;
    root->h = editor->height - 1;
    // FIXME(ibadawi): Just equalizing layout for now because it's easier
    window_equalize(root, WINDOW_SPLIT_HORIZONTAL);
    window_equalize(root, WINDOW_SPLIT_VERTICAL);
  } else {
    return true;
  }
  editor_draw(editor);
  return editor_waitkey(editor, ev);
}

char editor_getchar(struct editor *editor) {
  struct tb_event ev;
  editor_waitkey(editor, &ev);
  char ch = (char) ev.ch;
  if (ev.key == TB_KEY_SPACE) {
    ch = ' ';
  }
  return ch;
}

void editor_handle_key_press(struct editor *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
}

void editor_send_keys(struct editor *editor, const char *keys) {
  struct tb_event *last = NULL;
  for (const char *k = keys; *k; ++k) {
    struct tb_event *ev = xmalloc(sizeof(*ev));
    ev->key = 0;
    switch (*k) {
    case '<': {
      char key[10];
      size_t key_len = strcspn(k + 1, ">");
      strncpy(key, k + 1, key_len);
      key[key_len] = '\0';
      if (!strcmp("cr", key)) {
        ev->key = TB_KEY_ENTER;
      } else if (!strcmp("bs", key)) {
        ev->key = TB_KEY_BACKSPACE2;
      } else if (!strcmp("esc", key)) {
        ev->key = TB_KEY_ESC;
      } else {
        debug("BUG: editor_send_keys got <%s>\n", key);
        exit(1);
      }
      k += key_len + 1;
      break;
    }
    case ' ':
      ev->key = TB_KEY_SPACE;
      break;
    default:
      ev->ch = (uint32_t) *k;
      break;
    }
    // n.b. We want to correctly handle the case where editor_send_keys
    // indirectly recurses. For example 'o' is send_keys("A<cr>"), and 'A' is
    // send_keys("$i"). So we need to insert all the events in order, but
    // before any events that existed before we started this call. That's why
    // this isn't just a call to list_append.
    if (last) {
      list_insert_after(editor->synthetic_events, last, ev);
    } else {
      list_prepend(editor->synthetic_events, ev);
    }
    last = ev;
  }

  struct tb_event ev;
  while (!list_empty(editor->synthetic_events)) {
    bool ok = editor_waitkey(editor, &ev);
    assert(ok);
    editor_handle_key_press(editor, &ev);
  }
}

void editor_status_msg(struct editor *editor, const char *format, ...) {
  if (editor->status_silence) {
    return;
  }
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = false;
}

void editor_status_err(struct editor *editor, const char *format, ...) {
  if (editor->status_silence) {
    return;
  }
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = true;
}

void editor_undo(struct editor *editor) {
  if (!buffer_undo(editor->window->buffer)) {
    editor_status_msg(editor, "Already at oldest change");
  }
}

void editor_redo(struct editor *editor) {
  if (!buffer_redo(editor->window->buffer)) {
    editor_status_msg(editor, "Already at newest change");
  }
}
