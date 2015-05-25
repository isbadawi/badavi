#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <pwd.h>
#include <regex.h>
#include <unistd.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "mode.h"
#include "options.h"
#include "util.h"

#define R(n) {n, NULL}
static editor_register_t register_table[] = {
  // Last search pattern register
  // TODO(isbadawi): Investigate segfaults when this register is listed later.
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

static void editor_source_badavimrc(editor_t *editor) {
  const char *home = getenv("HOME");
  home = home ? home : getpwuid(getuid())->pw_dir;
  char cmd[255];
  snprintf(cmd, 255, "so %s/.badavimrc", home);
  editor_execute_command(editor, cmd);
  editor_status_msg(editor, "");
}

void editor_init(editor_t *editor) {
  editor->status = buf_create((size_t) (tb_width() / 2));
  editor->status_error = false;

  editor->buffers = list_create();
  buffer_t *buffer = buffer_create(NULL);
  list_append(editor->buffers, buffer);

  editor->windows = list_create();
  size_t width = (size_t) tb_width();
  size_t height = (size_t) tb_height();
  editor->window = window_create(buffer, 0, 0, width, height - 1);
  list_append(editor->windows, editor->window);

  editor->mode = normal_mode();

  editor->registers = register_table;
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    editor->registers[i].buf = buf_create(1);
  }

  editor->count = 0;
  editor->motion = NULL;
  editor->register_ = '"';

  editor_source_badavimrc(editor);
}

buf_t *editor_get_register(editor_t *editor, char name) {
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    if (editor->registers[i].name == name) {
      return editor->registers[i].buf;
    }
  }
  return NULL;
}

#define SEARCH_PATTERN_MAXLEN 256

// TODO(isbadawi): Searching should be a motion.
void editor_search(editor_t *editor) {
  buf_t *reg = editor_get_register(editor, '/');
  if (reg->len == 0) {
    editor_status_err(editor, "No previous regular expression");
    return;
  }

  gapbuf_t *gb = editor->window->buffer->text;
  size_t *cursor = &editor->window->cursor;

  char pattern[SEARCH_PATTERN_MAXLEN];
  memcpy(pattern, reg->buf, reg->len);
  pattern[reg->len] = '\0';

  gb_search_result_t result;
  gb_search_forwards(gb, pattern, *cursor + 1, &result);
  switch (result.status) {
  case GB_SEARCH_BAD_REGEX:
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, result.v.error);
    return;
  case GB_SEARCH_NO_MATCH:
    editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
    gb_search_forwards(gb, pattern, 0, &result);
    if (result.status == GB_SEARCH_NO_MATCH) {
      editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
      return;
    }
    // fallthrough
  case GB_SEARCH_MATCH:
    *cursor = result.v.match.start;
  }
}

static buffer_t *editor_get_buffer_by_name(editor_t* editor, char *name) {
  buffer_t *b;
  LIST_FOREACH(editor->buffers, b) {
    if (!strcmp(b->name, name)) {
      return b;
    }
  }
  return NULL;
}

void editor_open(editor_t *editor, char *path) {
  buffer_t *buffer = editor_get_buffer_by_name(editor, path);

  if (!buffer) {
    if (access(path, F_OK) < 0) {
      buffer = buffer_create(path);
      editor_status_msg(editor, "\"%s\" [New File]", path);
    } else {
      buffer = buffer_open(path);
      editor_status_msg(editor, "\"%s\" %zu, %zu",
          path, gb_nlines(buffer->text), gb_size(buffer->text));
    }
    list_append(editor->buffers, buffer);
  }
  editor->window->buffer = buffer;
}

void editor_push_mode(editor_t *editor, editing_mode_t *mode) {
  mode->parent = editor->mode;
  editor->mode = mode;
  editor->mode->entered(editor);
}

void editor_pop_mode(editor_t *editor) {
  if (editor->mode->parent) {
    editor->mode = editor->mode->parent;
    editor->mode->entered(editor);
  }
}

void editor_save_buffer(editor_t *editor, char *path) {
  buffer_t *buffer = editor->window->buffer;
  char *name;
  int rc;
  if (path) {
    rc = buffer_saveas(buffer, path);
    name = path;
  } else {
    rc = buffer_write(buffer);
    name = buffer->name;
  }
  if (!rc) {
    editor_status_msg(editor, "\"%s\" %zuL, %zuC written",
        name, gb_nlines(buffer->text), gb_size(buffer->text));
  } else {
    editor_status_err(editor, "No file name");
  }
}

static void editor_equalize_windows(editor_t *editor) {
  size_t nwindows = list_size(editor->windows);
  size_t width = ((size_t) tb_width() - nwindows) / nwindows;
  size_t height = (size_t) tb_height() - ((nwindows == 1) ? 1 : 2);

  size_t i = 0;
  window_t *last = NULL;
  window_t *w;
  LIST_FOREACH(editor->windows, w) {
    w->x = i++ * (width + 1);
    w->y = 0;
    w->w = width;
    w->h = height;
    last = w;
  }

  assert(last);
  last->w += ((size_t) tb_width() - nwindows) % nwindows + 1;
}

typedef struct {
  const char *name;
  void (*action)(editor_t*, char*);
} editor_command_t;

static void editor_command_quit(editor_t *editor, char __unused *arg) {
  buffer_t *b;
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
static void editor_command_force_quit(editor_t __unused *editor,
                                      char __unused *arg) {
  exit(0);
}

static void editor_command_force_close_window(editor_t *editor, char *arg) {
  if (list_size(editor->windows) == 1) {
    editor_command_force_quit(editor, arg);
  }

  window_t *window = list_prev(editor->windows, editor->window);
  if (!window) {
    window = list_next(editor->windows, editor->window);
  }
  list_remove(editor->windows, editor->window);
  free(editor->window);
  editor->window = window;
  editor_equalize_windows(editor);
}

static void editor_command_close_window(editor_t *editor, char *arg) {
  if (list_size(editor->windows) == 1) {
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

static void editor_command_write_quit(editor_t *editor, char *arg) {
  editor_save_buffer(editor, NULL);
  editor_command_close_window(editor, arg);
}

static void editor_command_edit(editor_t *editor, char *arg) {
  if (arg) {
    editor_open(editor, arg);
  } else {
    editor_status_err(editor, "No file name");
  }
}

static void editor_command_source(editor_t *editor, char *arg) {
  if (!arg) {
    editor_status_err(editor, "Argument required");
    return;
  }

  FILE *fp = fopen(arg, "r");
  if (!fp) {
    editor_status_err(editor, "Can't open file %s", arg);
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

static void editor_command_set(editor_t *editor, char *arg) {
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

window_t *editor_left_window(editor_t *editor, window_t *window) {
  return list_prev(editor->windows, window);
}

window_t *editor_right_window(editor_t *editor, window_t *window) {
  return list_next(editor->windows, window);
}

static void editor_command_vsplit(editor_t *editor, char *arg) {
  window_t *window = window_create(editor->window->buffer, 0, 0, 0, 0);
  if (option_get_bool("splitright")) {
    list_insert_after(editor->windows, editor->window, window);
  } else {
    list_insert_before(editor->windows, editor->window, window);
  }
  editor->window = window;

  editor_equalize_windows(editor);

  if (arg) {
    editor_command_edit(editor, arg);
  }
}

static editor_command_t editor_commands[] = {
  {"q", editor_command_close_window},
  {"q!", editor_command_force_close_window},
  {"qa", editor_command_quit},
  {"qa!", editor_command_force_quit},
  {"w", editor_save_buffer},
  {"wq", editor_command_write_quit},
  {"e", editor_command_edit},
  {"so", editor_command_source},
  {"set", editor_command_set},
  {"vsp", editor_command_vsplit},
  {NULL, NULL}
};

void editor_execute_command(editor_t *editor, char *command) {
  if (!*command) {
    return;
  }
  char *cmd = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  for (int i = 0; editor_commands[i].name; ++i) {
    if (!strcmp(cmd, editor_commands[i].name)) {
      editor_commands[i].action(editor, arg);
      return;
    }
  }
  editor_status_err(editor, "Not an editor command: %s", command);
}

void editor_draw(editor_t *editor) {
  tb_clear();

  bool first = true;
  bool drawplate = list_size(editor->windows) > 1;

  window_t *w;
  LIST_FOREACH(editor->windows, w) {
    if (!first) {
      for (int y = 0; y < tb_height() - 1; ++y) {
        tb_change_cell((int) w->x - 1, y, '|', TB_BLACK, TB_WHITE);
      }
    }
    first = false;

    window_draw(w);
    if (w == editor->window) {
      window_draw_cursor(w);
    }

    if (drawplate) {
      char plate[300];
      strcpy(plate, *w->buffer->name ? w->buffer->name : "[No Name]");
      if (w->buffer->dirty) {
        strcat(plate, " [+]");
      }
      size_t platelen = strlen(plate);
      for (size_t x = 0; x < w->w; ++x) {
        char c = x < platelen ? plate[x] : ' ';
        tb_change_cell((int) (w->x + x), tb_height() - 2, (uint32_t) c,
                       TB_BLACK, TB_WHITE);
      }
    }
  }

  for (size_t x = 0; x < editor->status->len; ++x) {
    tb_change_cell((int) x, tb_height() - 1, (uint32_t) editor->status->buf[x],
        editor->status_error ? TB_DEFAULT : TB_WHITE,
        editor->status_error ? TB_RED : TB_DEFAULT);
  }

  tb_present();
}

// The editor handles key presses by delegating to its mode.
void editor_handle_key_press(editor_t *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
  editor_draw(editor);
}

void editor_send_keys(editor_t *editor, const char *keys) {
  size_t len = strlen(keys);
  struct tb_event ev;
  for (size_t i = 0; i < len; ++i) {
    ev.key = 0;
    switch (keys[i]) {
    case '<': {
      char key[10];
      size_t key_len = strcspn(keys + i + 1, ">");
      strncpy(key, keys + i + 1, key_len);
      key[key_len] = '\0';
      if (!strcmp("cr", key)) {
        ev.key = TB_KEY_ENTER;
      } else if (!strcmp("bs", key)) {
        ev.key = TB_KEY_BACKSPACE2;
      } else if (!strcmp("esc", key)) {
        ev.key = TB_KEY_ESC;
      } else {
        debug("BUG: editor_send_keys got <%s>\n", key);
        exit(1);
      }
      i += key_len + 1;
      break;
    }
    case ' ':
      ev.key = TB_KEY_SPACE;
      break;
    default:
      ev.ch = (uint32_t) keys[i];
      break;
    }
    editor_handle_key_press(editor, &ev);
  }
}

void editor_status_msg(editor_t *editor, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = false;
}

void editor_status_err(editor_t *editor, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = true;
}

void editor_undo(editor_t* editor) {
  buffer_t *buffer = editor->window->buffer;
  if (list_empty(buffer->undo_stack)) {
    editor_status_msg(editor, "Already at oldest change");
    return;
  }

  list_t *group = list_pop(buffer->undo_stack);

  gapbuf_t *gb = buffer->text;
  edit_action_t *action;
  LIST_FOREACH(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      editor->window->cursor = action->pos;
      break;
    case EDIT_ACTION_DELETE:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      editor->window->cursor = action->pos;
      break;
    }
  }

  list_prepend(buffer->redo_stack, group);
}

void editor_redo(editor_t* editor) {
  buffer_t *buffer = editor->window->buffer;
  if (list_empty(buffer->redo_stack)) {
    editor_status_msg(editor, "Already at newest change");
    return;
  }

  list_t *group = list_pop(buffer->redo_stack);

  gapbuf_t *gb = buffer->text;
  edit_action_t *action;
  LIST_FOREACH_REVERSE(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      editor->window->cursor = action->pos + action->buf->len;
      break;
    case EDIT_ACTION_DELETE:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      editor->window->cursor = action->pos;
      break;
    }
  }

  list_prepend(buffer->undo_stack, group);
}

void editor_start_action_group(editor_t *editor) {
  buffer_t *buffer = editor->window->buffer;
  list_t *group;
  LIST_FOREACH(buffer->redo_stack, group) {
    edit_action_t *action;
    LIST_FOREACH(group, action) {
      buf_free(action->buf);
      free(action);
    }
    list_free(group);
  }
  list_clear(buffer->redo_stack);

  list_prepend(buffer->undo_stack, list_create());
}

void editor_add_action(editor_t *editor, edit_action_t action) {
  buffer_t *buffer = editor->window->buffer;
  edit_action_t *action_copy = malloc(sizeof(edit_action_t));
  memcpy(action_copy, &action, sizeof action);
  list_t *group = list_peek(buffer->undo_stack);
  list_prepend(group, action_copy);
}
