#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <regex.h>
#include <unistd.h>

#include <libclipboard.h>
#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "list.h"
#include "mode.h"
#include "tags.h"
#include "terminal.h"
#include "util.h"
#include "window.h"

static clipboard_c *clipboard = NULL;

static char* register_buffer_read(struct editor_register *reg) {
  return strdup(reg->buf->buf);
}

static void register_buffer_write(struct editor_register *reg, char *text) {
  buf_clear(reg->buf);
  buf_append(reg->buf, text);
}

static char *register_clipboard_read(struct editor_register *reg) {
  assert(reg->name == '*' || reg->name == '+');
  clipboard_mode mode = reg->name == '+' ? LCB_CLIPBOARD : LCB_SELECTION;
  return clipboard_text_ex(clipboard, NULL, mode);
}

static void register_clipboard_write(struct editor_register *reg, char *text) {
  assert(reg->name == '*' || reg->name == '+');
  clipboard_mode mode = reg->name == '+' ? LCB_CLIPBOARD : LCB_SELECTION;
  bool rc = clipboard_set_text_ex(clipboard, text, -1, mode);
  assert(rc);
}

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

  editor->highlight_search_matches = true;
  editor->mode = normal_mode();

  if (!clipboard) {
    clipboard = clipboard_new(NULL);
  }

#define R(n) \
  editor->registers[next_register++] = (struct editor_register) \
      {n, buf_create(1), register_buffer_read, register_buffer_write};
  int next_register = 0;
  // Named registers
  for (char c = 'a'; c <= 'z'; ++c) {
    R(c);
  }
  // Last search pattern register
  R('/');
  // Unnamed default register
  R('"');
  // Global clipboard register
  editor->registers[next_register++] = (struct editor_register)
    {'+', NULL, register_clipboard_read, register_clipboard_write};
  // Selection clipboard register
  editor->registers[next_register++] = (struct editor_register)
    {'*', NULL, register_clipboard_read, register_clipboard_write};
  assert(next_register == EDITOR_NUM_REGISTERS);
#undef R

  editor->tags = tags_create("tags");

  editor->count = 0;
  editor->register_ = '"';

  editor->synthetic_events = list_create();

#define OPTION(name, _, defaultval) \
  editor->opt.name = defaultval;
  BUFFER_OPTIONS
  EDITOR_OPTIONS
#undef OPTION
}

struct editor_register *editor_get_register(struct editor *editor, char name) {
  for (int i = 0; i < EDITOR_NUM_REGISTERS; ++i) {
    if (editor->registers[i].name == name) {
      return &editor->registers[i];
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
    memcpy(&buffer->opt, &editor->opt, sizeof(buffer->opt));
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

static void editor_command_quit(struct editor *editor, char *arg ATTR_UNUSED) {
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

ATTR_NORETURN
static void editor_command_force_quit(
    struct editor *editor ATTR_UNUSED, char *arg ATTR_UNUSED) {
  exit(0);
}

static void editor_command_force_close_window(struct editor *editor, char *arg) {
  if (window_root(editor->window)->split_type == WINDOW_LEAF) {
    editor_command_force_quit(editor, arg);
  }
  assert(editor->window->parent);

  enum window_split_type split_type = editor->window->parent->split_type;
  editor->window = window_close(editor->window);

  if (editor->opt.equalalways) {
    window_equalize(editor->window, split_type);
  }
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

static void *editor_opt_val(struct editor *editor, struct opt *info,
                            bool global) {
  switch (info->scope) {
  case OPTION_SCOPE_WINDOW:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      return &editor->window->opt.optname; \
    }
    WINDOW_OPTIONS
#undef OPTION
    break;
  case OPTION_SCOPE_BUFFER:
    if (!global) {
#define OPTION(optname, _, __) \
      if (!strcmp(info->name, #optname)) { \
        return &editor->window->buffer->opt.optname; \
      }
      BUFFER_OPTIONS
#undef OPTION
    }
    // fallthrough
  case OPTION_SCOPE_EDITOR:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      return &editor->opt.optname; \
    }
    BUFFER_OPTIONS
    EDITOR_OPTIONS
#undef OPTION
  }

  assert(0);
  return NULL;
}

static void editor_command_set_impl(struct editor *editor, char *arg,
                                    bool global) {
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

  struct opt *info = option_info(opt);

  if (!info) {
    editor_status_err(editor, "Unknown option: %s", opt);
    return;
  }

  void *val = editor_opt_val(editor, info, global);
  bool *boolval = (bool*)val;
  int *intval = (int*)val;

  if (groups[3].rm_so == -1) {
    if (info->type == OPTION_TYPE_int) {
      editor_status_msg(editor, "%s=%d", opt, *intval);
    } else if (info->type == OPTION_TYPE_bool) {
      *boolval = groups[1].rm_so == -1;
    }
    return;
  }

  switch (arg[groups[3].rm_so]) {
  case '!':
    if (info->type == OPTION_TYPE_bool) {
      *boolval = !*boolval;
    } else {
      editor_status_err(editor, "Invalid argument: %s", arg);
    }
    break;
  case '?':
    if (info->type == OPTION_TYPE_int) {
      editor_status_msg(editor, "%s=%d", opt, *intval);
    } else if (info->type == OPTION_TYPE_bool) {
      editor_status_msg(editor, "%s%s", *boolval ? "" : "no", opt);
    }
    break;
  case '=':
    if (info->type == OPTION_TYPE_int) {
      *intval = atoi(arg + groups[3].rm_so + 1);
    } else {
      editor_status_err(editor, "Invalid argument: %s", arg);
    }
    break;
  }
  return;
}

static void editor_command_setglobal(struct editor *editor, char *arg) {
  editor_command_set_impl(editor, arg, true);
}

static void editor_command_setlocal(struct editor *editor, char *arg) {
  editor_command_set_impl(editor, arg, false);
}

static void editor_command_set(struct editor *editor, char *arg) {
  editor_command_setglobal(editor, arg);
  editor_command_setlocal(editor, arg);
}

static void editor_command_split(struct editor *editor, char *arg) {
  editor->window = window_split(editor->window,
      editor->opt.splitbelow ? WINDOW_SPLIT_BELOW : WINDOW_SPLIT_ABOVE);

  if (editor->opt.equalalways) {
    window_equalize(editor->window, WINDOW_SPLIT_HORIZONTAL);
  }

  if (arg) {
    editor_command_edit(editor, arg);
  }
}

static void editor_command_vsplit(struct editor *editor, char *arg) {
  editor->window = window_split(editor->window,
      editor->opt.splitright ? WINDOW_SPLIT_RIGHT : WINDOW_SPLIT_LEFT);

  if (editor->opt.equalalways) {
    window_equalize(editor->window, WINDOW_SPLIT_VERTICAL);
  }

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

static void editor_command_nohlsearch(
    struct editor *editor, char *arg ATTR_UNUSED) {
  editor->highlight_search_matches = false;
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
  {"setlocal", "setl", editor_command_setlocal},
  {"setglobal", "setg", editor_command_setglobal},
  {"split", "sp", editor_command_split},
  {"vsplit", "vsp", editor_command_vsplit},
  {"tag", "tag", editor_command_tag},
  {"nohlsearch", "noh", editor_command_nohlsearch},
  {NULL, NULL, NULL}
};

void editor_execute_command(struct editor *editor, char *command) {
  if (!*command) {
    return;
  }
  char *copy = xstrdup(command);
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
    editor_status_err(editor, "Not an editor command: %s", copy);
  }
  free(copy);
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
    ev->type = TB_EVENT_KEY;
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
