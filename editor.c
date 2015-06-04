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
#include "list.h"
#include "mode.h"
#include "tags.h"
#include "options.h"
#include "window.h"
#include "util.h"

#define R(n) {n, NULL}
static struct editor_register_t register_table[] = {
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

static void editor_source_badavimrc(struct editor_t *editor) {
  const char *home = getenv("HOME");
  home = home ? home : getpwuid(getuid())->pw_dir;
  char cmd[255];
  snprintf(cmd, 255, "source %s/.badavimrc", home);
  editor_execute_command(editor, cmd);
  editor_status_msg(editor, "");
}

void editor_init(struct editor_t *editor) {
  editor->buffers = list_create();
  struct buffer_t *buffer = buffer_create(NULL);
  list_append(editor->buffers, buffer);

  editor->windows = list_create();
  editor->width = (size_t) tb_width();
  editor->height = (size_t) tb_height();
  editor->window = window_create(buffer, 0, 0, editor->width, editor->height - 1);
  list_append(editor->windows, editor->window);

  editor->status = buf_create(editor->width);
  editor->status_error = false;

  editor->mode = normal_mode();

  editor->registers = register_table;
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    editor->registers[i].buf = buf_create(1);
  }

  editor->tags = tags_create("tags");

  editor->count = 0;
  editor->motion = NULL;
  editor->register_ = '"';

  editor_source_badavimrc(editor);
}

struct buf_t *editor_get_register(struct editor_t *editor, char name) {
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    if (editor->registers[i].name == name) {
      return editor->registers[i].buf;
    }
  }
  return NULL;
}

#define SEARCH_PATTERN_MAXLEN 256

// TODO(isbadawi): Searching should be a motion.
void editor_search(struct editor_t *editor, enum editor_search_direction_t direction) {
  struct buf_t *reg = editor_get_register(editor, '/');
  if (reg->len == 0) {
    editor_status_err(editor, "No previous regular expression");
    return;
  }

  char pattern[SEARCH_PATTERN_MAXLEN];
  memcpy(pattern, reg->buf, reg->len);
  pattern[reg->len] = '\0';

  struct gapbuf_t *gb = editor->window->buffer->text;
  struct gb_search_result_t result;
  gb_search(gb, pattern, &result);

  if (!result.matches) {
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, result.error);
    return;
  }

  if (list_empty(result.matches)) {
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return;
  }

  struct gb_match_t *match = NULL;
  struct gb_match_t *m;
  if (direction == EDITOR_SEARCH_FORWARDS) {
    LIST_FOREACH(result.matches, m) {
      if (m->start > editor->window->cursor) {
        match = m;
        break;
      }
    }

    if (!match) {
      editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
      match = result.matches->head->next->data;
    }
  } else {
    struct gb_match_t *last = NULL;
    LIST_FOREACH(result.matches, m) {
      if (last && last->start < editor->window->cursor &&
          m->start >= editor->window->cursor) {
        match = last;
        break;
      }
      last = m;
    }
    if (last->start < editor->window->cursor) {
      match = last;
    }
    if (!match) {
      editor_status_msg(editor, "search hit TOP, continuing at BOTTOM");
      match = result.matches->tail->prev->data;
    }
  }

  editor->window->cursor = match->start;

  // TODO(isbadawi): Remember matches -- can do hlsearch, or cache searches
  // if the buffer hasn't changed.
  LIST_FOREACH(result.matches, m) {
    free(m);
  }
  list_clear(result.matches);
}

static struct buffer_t *editor_get_buffer_by_name(struct editor_t* editor, char *name) {
  struct buffer_t *b;
  LIST_FOREACH(editor->buffers, b) {
    if (!strcmp(b->name, name)) {
      return b;
    }
  }
  return NULL;
}

void editor_open(struct editor_t *editor, char *path) {
  struct buffer_t *buffer = editor_get_buffer_by_name(editor, path);

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
  editor->window->buffer = buffer;
  editor->window->cursor = 0;
  editor->window->top = 0;
  editor->window->left = 0;
}

void editor_push_mode(struct editor_t *editor, struct editing_mode_t *mode) {
  mode->parent = editor->mode;
  editor->mode = mode;
  if (editor->mode->entered) {
    editor->mode->entered(editor);
  }
}

void editor_pop_mode(struct editor_t *editor) {
  if (editor->mode->parent) {
    editor->mode = editor->mode->parent;
    if (editor->mode->entered) {
      editor->mode->entered(editor);
    }
  }
}

void editor_save_buffer(struct editor_t *editor, char *path) {
  struct buffer_t *buffer = editor->window->buffer;
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

void editor_equalize_windows(struct editor_t *editor) {
  size_t nwindows = list_size(editor->windows);
  size_t width = (editor->width - nwindows) / nwindows;
  size_t height = editor->height - ((nwindows == 1) ? 1 : 2);

  size_t i = 0;
  struct window_t *last = NULL;
  struct window_t *w;
  LIST_FOREACH(editor->windows, w) {
    w->x = i++ * (width + 1);
    w->y = 0;
    w->w = width;
    w->h = height;
    last = w;
  }

  assert(last);
  last->w += (editor->width - nwindows) % nwindows + 1;
}

struct editor_command_t {
  const char *name;
  const char *shortname;
  void (*action)(struct editor_t*, char*);
};

static void editor_command_quit(struct editor_t *editor, char __unused *arg) {
  struct buffer_t *b;
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
static void editor_command_force_quit(struct editor_t __unused *editor,
                                      char __unused *arg) {
  exit(0);
}

static void editor_command_force_close_window(struct editor_t *editor, char *arg) {
  if (list_size(editor->windows) == 1) {
    editor_command_force_quit(editor, arg);
  }

  struct window_t *window = list_prev(editor->windows, editor->window);
  if (!window) {
    window = list_next(editor->windows, editor->window);
  }
  list_remove(editor->windows, editor->window);
  free(editor->window);
  editor->window = window;
  editor_equalize_windows(editor);
}

static void editor_command_close_window(struct editor_t *editor, char *arg) {
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

static void editor_command_write_quit(struct editor_t *editor, char *arg) {
  editor_save_buffer(editor, NULL);
  editor_command_close_window(editor, arg);
}

static void editor_command_edit(struct editor_t *editor, char *arg) {
  if (arg) {
    editor_open(editor, arg);
  } else {
    editor_status_err(editor, "No file name");
  }
}

static void editor_command_source(struct editor_t *editor, char *arg) {
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

static void editor_command_set(struct editor_t *editor, char *arg) {
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

struct window_t *editor_left_window(struct editor_t *editor, struct window_t *window) {
  return list_prev(editor->windows, window);
}

struct window_t *editor_right_window(struct editor_t *editor, struct window_t *window) {
  return list_next(editor->windows, window);
}

static void editor_command_vsplit(struct editor_t *editor, char *arg) {
  struct window_t *window = window_create(editor->window->buffer, 0, 0, 0, 0);
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

static void editor_command_tag(struct editor_t *editor, char *arg) {
  if (!arg) {
    editor_tag_stack_next(editor);
  } else {
    editor_jump_to_tag(editor, arg);
  }
}

static struct editor_command_t editor_commands[] = {
  {"quit", "q", editor_command_close_window},
  {"quit!", "q!", editor_command_force_close_window},
  {"qall", "qa", editor_command_quit},
  {"qall!", "qa!", editor_command_force_quit},
  {"write", "w", editor_save_buffer},
  {"wq", "wq", editor_command_write_quit},
  {"edit", "e", editor_command_edit},
  {"source", "so", editor_command_source},
  {"set", "set", editor_command_set},
  {"vsplit", "vsp", editor_command_vsplit},
  {"tag", "tag", editor_command_tag},
  {NULL, NULL, NULL}
};

void editor_execute_command(struct editor_t *editor, char *command) {
  if (!*command) {
    return;
  }
  char *name = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  for (int i = 0; editor_commands[i].name; ++i) {
    struct editor_command_t *cmd = &editor_commands[i];
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

void editor_draw(struct editor_t *editor) {
  tb_clear();

  bool first = true;
  bool drawplate = list_size(editor->windows) > 1;

  struct window_t *w;
  LIST_FOREACH(editor->windows, w) {
    if (!first) {
      for (size_t y = 0; y < editor->height - 1; ++y) {
        tb_change_cell((int) w->x - 1, (int) y, '|', TB_BLACK, TB_WHITE);
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
        tb_change_cell((int) (w->x + x), (int) editor->height - 2, (uint32_t) c,
                       TB_BLACK, TB_WHITE);
      }
    }
  }

  for (size_t x = 0; x < editor->status->len; ++x) {
    tb_change_cell((int) x, (int) editor->height - 1, (uint32_t) editor->status->buf[x],
        editor->status_error ? TB_DEFAULT : TB_WHITE,
        editor->status_error ? TB_RED : TB_DEFAULT);
  }

  tb_present();
}

void editor_handle_key_press(struct editor_t *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
}

void editor_send_keys(struct editor_t *editor, char *keys) {
  struct tb_event ev;
  for (char *k = keys; *k; ++k) {
    ev.key = 0;
    switch (*k) {
    case '<': {
      char key[10];
      size_t key_len = strcspn(k + 1, ">");
      strncpy(key, k + 1, key_len);
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
      k += key_len + 1;
      break;
    }
    case ' ':
      ev.key = TB_KEY_SPACE;
      break;
    default:
      ev.ch = (uint32_t) *k;
      break;
    }
    editor_handle_key_press(editor, &ev);
  }
}

void editor_status_msg(struct editor_t *editor, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = false;
}

void editor_status_err(struct editor_t *editor, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = true;
}
