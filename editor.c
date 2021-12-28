#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>

#include <libclipboard.h>
#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "mode.h"
#include "tags.h"
#include "terminal.h"
#include "util.h"
#include "window.h"

TAILQ_HEAD(editor_command_list, editor_command) editor_commands;
bool commands_init_done = false;
int num_commands = 0;

void register_editor_command(struct editor_command *command) {
  if (!commands_init_done) {
    TAILQ_INIT(&editor_commands);
    commands_init_done = true;
  }
  TAILQ_INSERT_TAIL(&editor_commands, command, pointers);
  ++num_commands;
}

static int pstrcmp(const void *a, const void *b) {
  return strcmp(*(char * const *)a, *(char * const *)b);
}

char **commands_get_sorted(int *len) {
  assert(commands_init_done);
  char **commands = xmalloc(sizeof(*commands) * num_commands);
  int i = 0;

  struct editor_command *command;
  TAILQ_FOREACH(command, &editor_commands, pointers) {
    commands[i++] = (char*) command->name;
  }

  qsort(commands, num_commands, sizeof(*commands), pstrcmp);
  *len = num_commands;

  return commands;
}

static clipboard_c *clipboard = NULL;

static char* register_buffer_read(struct editor_register *reg) {
  return xstrdup(reg->buf->buf);
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

void editor_free(struct editor *editor) {
  buf_free(editor->message);
  buf_free(editor->status);
  free(editor->pwd);
  for (int i = 0; i < EDITOR_NUM_REGISTERS; ++i) {
    if (editor->registers[i].buf) {
      buf_free(editor->registers[i].buf);
    }
  }
  tags_clear(editor->tags);
  free(editor->tags);
  window_free(window_root(editor->window));
  struct buffer *b, *tb;
  TAILQ_FOREACH_SAFE(b, &editor->buffers, pointers, tb) {
    buffer_free(b);
  }
  editor_free_options(editor);
  history_deinit(&editor->command_history);
  history_deinit(&editor->search_history);
  free(editor);
}

struct editor *editor_create_and_open(size_t width, size_t height, char *path) {
  struct editor *editor = xmalloc(sizeof(*editor));

  editor->width = width;
  editor->height = height;

  editor->message = buf_create(1);
  editor->status = buf_create(editor->width);
  editor->status_error = false;
  editor->status_silence = false;
  editor->status_cursor = 0;

  memset(&editor->popup, 0, sizeof(editor->popup));

  editor->highlight_search_matches = false;
  editor->recording = '\0';

  memset(&editor->modes, 0, sizeof(editor->modes));
#define MODE(name) do { \
    struct editing_mode *mode = (struct editing_mode*) &editor->modes.name; \
    mode->kind = EDITING_MODE_##name; \
    mode->entered = name##_mode_entered; \
    mode->exited = name##_mode_exited; \
    mode->key_pressed = name##_mode_key_pressed; \
  } while (0);
  MODES
#undef MODE

  editor->mode = NULL;
  editor_push_normal_mode(editor, 0);

  editor->pwd = NULL;

  if (!clipboard) {
    clipboard = clipboard_new(NULL);
  }

#define R(n) \
  editor->registers[next_register++] = (struct editor_register) \
      {n, buf_create(1), register_buffer_read, register_buffer_write}
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

  TAILQ_INIT(&editor->synthetic_events);

  editor_init_options(editor);

  editor->window = window_create(NULL, editor->width, editor->height - 1);

  TAILQ_INIT(&editor->buffers);
  if (!path) {
    struct buffer *buffer = buffer_create(NULL);
    buffer_inherit_editor_options(buffer, editor);
    TAILQ_INSERT_TAIL(&editor->buffers, buffer, pointers);
    window_set_buffer(editor->window, buffer);
  } else {
    editor_open(editor, path);
  }

  history_init(&editor->command_history, &editor->opt.history);
  history_init(&editor->search_history, &editor->opt.history);

  return editor;
}

struct editor *editor_create(size_t width, size_t height) {
  return editor_create_and_open(width, height, NULL);
}

struct editor_register *editor_get_register(struct editor *editor, char name) {
  for (int i = 0; i < EDITOR_NUM_REGISTERS; ++i) {
    if (editor->registers[i].name == name) {
      return &editor->registers[i];
    }
  }
  return NULL;
}

static struct buffer *editor_get_buffer_by_path(struct editor* editor, char *name) {
  struct buffer *b;
  TAILQ_FOREACH(b, &editor->buffers, pointers) {
    if (b->path && !strcmp(b->path, name)) {
      return b;
    }
  }
  return NULL;
}

static void editor_status_buffer_info(struct editor *editor, struct buffer *buffer) {
  editor_status_msg(editor, "\"%s\" %s%zuL, %zuC",
      editor_relpath(editor, buffer->path),
      buffer->opt.readonly ? "[readonly] " : "",
      gb_nlines(buffer->text),
      gb_size(buffer->text));
}

void editor_open(struct editor *editor, char *path) {
  path = abspath(path);
  struct buffer *buffer = editor_get_buffer_by_path(editor, path);
  if (buffer) {
    free(path);
    editor_status_buffer_info(editor, buffer);
    window_set_buffer(editor->window, buffer);
    return;
  }

  buffer = buffer_open(path);
  int open_errno = errno;
  if (buffer) {
    buffer_inherit_editor_options(buffer, editor);
    buffer->opt.readonly = access(path, W_OK) < 0;
    editor_status_buffer_info(editor, buffer);
  } else {
    buffer = buffer_create(path);
    buffer_inherit_editor_options(buffer, editor);
    const char *rel = editor_relpath(editor, path);
    switch (open_errno) {
    case ENOENT:
      editor_status_msg(editor, "\"%s\" [New File]", rel);
      break;
    case EACCES:
      buffer->opt.readonly = true;
      editor_status_msg(editor, "\"%s\" [Permission Denied]", rel);
      break;
    default: break;
    }
  }

  free(path);
  TAILQ_INSERT_TAIL(&editor->buffers, buffer, pointers);
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

bool editor_save_buffer(struct editor *editor, char *path) {
  struct buffer *buffer = editor->window->buffer;
  if (!path && !buffer->path) {
    editor_status_err(editor, "No file name");
    return false;
  }

  bool rc;
  const char *name;
  if (path) {
    path = abspath(path);
    rc = buffer_saveas(buffer, path);
    if (rc && !buffer->path) {
      buffer->path = path;
    }
    name = path;
  } else {
    rc = buffer_write(buffer);
    name = editor_relpath(editor, buffer->path);
  }
  if (rc) {
    editor_status_msg(editor, "\"%s\" %zuL, %zuC written",
        name, gb_nlines(buffer->text), gb_size(buffer->text));
  } else if (errno) {
    editor_status_err(editor, "%s", strerror(errno));
  }

  free(path);
  return rc;
}

const char *editor_buffer_name(struct editor *editor, struct buffer *buffer) {
  return buffer->path ? editor_relpath(editor, buffer->path) : "[No Name]";
}

EDITOR_COMMAND(qall, qa) {
  if (!force) {
    struct buffer *buffer;
    TAILQ_FOREACH(buffer, &editor->buffers, pointers) {
      if (buffer->opt.modified) {
        const char *path = editor_buffer_name(editor, buffer);
        editor_status_err(editor,
            "No write since last change for buffer \"%s\"", path);
        return;
      }
    }
  }
  exit(0);
}

EDITOR_COMMAND(quit, q) {
  if (window_root(editor->window)->split_type == WINDOW_LEAF) {
    editor_command_qall(editor, arg, force);
    return;
  }
  assert(editor->window->parent);

  if (!force && editor->window->buffer->opt.modified) {
    editor_status_err(editor,
        "No write since last change (add ! to override)");
    return;
  }

  enum window_split_type split_type = editor->window->parent->split_type;
  struct window *old_window = editor->window;
  editor_set_window(editor, window_close(editor->window));
  window_free(old_window);

  if (editor->opt.equalalways) {
    window_equalize(editor->window, split_type);
  }
}

EDITOR_COMMAND(wq, wq) {
  if (!force && editor->window->buffer->opt.readonly) {
    editor_status_err(editor, "'readonly' option is set (add ! to override)");
    return;
  }

  editor->window->buffer->opt.readonly = false;
  if (editor_save_buffer(editor, NULL)) {
    editor_command_quit(editor, arg, false);
  }
}

EDITOR_COMMAND_WITH_COMPLETION(write, w, COMPLETION_PATHS) {
  if (!force && editor->window->buffer->opt.readonly) {
    editor_status_err(editor, "'readonly' option is set (add ! to override)");
    return;
  }

  editor->window->buffer->opt.readonly = false;
  editor_save_buffer(editor, arg);
}

EDITOR_COMMAND_WITH_COMPLETION(edit, e, COMPLETION_PATHS) {
  if (arg) {
    editor_open(editor, arg);
  } else {
    editor_status_err(editor, "No file name");
  }
}

static bool window_any(
    struct window *window, bool (*f)(struct window*, void*), void *ctx) {
  if (window->split_type == WINDOW_LEAF) {
    return f(window, ctx);
  }
  return window_any(window->split.first, f, ctx) ||
    window_any(window->split.second, f, ctx);
}

static bool window_buffer_is(struct window* window, void *ctx) {
  return window->buffer == ctx;
}

static bool editor_buffer_is_active(struct editor *editor, struct buffer *buffer) {
  return window_any(window_root(editor->window), window_buffer_is, buffer);
}

EDITOR_COMMAND(buffers, ls) {
  int i = 1;
  buf_clear(editor->message);
  char *alt = editor->window->alternate_path;
  struct buffer *buffer;
  TAILQ_FOREACH(buffer, &editor->buffers, pointers) {
    char flags[6] = {' ', ' ', ' ', ' ', ' ', '\0'};
    flags[0] = alt && buffer->path && !strcmp(alt, buffer->path)? '#' : flags[0];
    flags[0] = buffer == editor->window->buffer ? '%' : flags[0];
    flags[1] = editor_buffer_is_active(editor, buffer) ? 'a' : 'h';
    flags[2] = buffer->opt.readonly ? '=' : flags[2];
    flags[2] = !buffer->opt.modifiable ? '-' : flags[2];
    flags[3] = buffer->opt.modified ? '+' : flags[3];
    const char *path = editor_buffer_name(editor, buffer);
    buf_appendf(editor->message, "\n  %d %s \"%s\"", i++, flags, path);
  }
  editor_status_msg(editor, "Press ENTER to continue ");
  editor->status_cursor = editor->status->len - 1;
}

EDITOR_COMMAND(registers, reg) {
  buf_clear(editor->message);
  buf_appendf(editor->message, "\n Type Name Content");
  for (int i = 0; i < EDITOR_NUM_REGISTERS; ++i) {
    struct editor_register *reg = &editor->registers[i];
    if (arg && !strchr(arg, reg->name)) {
      continue;
    }
    char *contents = reg->read(reg);
    if (*contents) {
      buf_appendf(editor->message, "\n  c   \"%c   ", reg->name);
      for (char *c = contents; *c; ++c) {
        if (*c < 32 || *c == 127) {
          // Escape using caret notation.
          buf_appendf(editor->message, "^%c", *c ^ 0x40);
        } else {
          buf_appendf(editor->message, "%c", *c);
        }
      }
    }
    free(contents);
  }
  editor_status_msg(editor, "Press ENTER to continue ");
  editor->status_cursor = editor->status->len - 1;
}

static char cwdbuffer[PATH_MAX] = {0};
static char *editor_working_directory(struct editor *editor) {
  if (editor->window->pwd) {
    return editor->window->pwd;
  }

  if (editor->pwd) {
    return editor->pwd;
  }

  getcwd(cwdbuffer, sizeof(cwdbuffer));
  return cwdbuffer;
}

const char *editor_relpath(struct editor *editor, const char *path) {
  return relpath(path, editor_working_directory(editor));
}

static char *editor_find_in_path_verbatim(struct editor *editor, char *file) {
  char buf[PATH_MAX];

  char *p, *dirs;
  p = dirs = xstrdup(editor->opt.path);

  char *dir;
  while ((dir = strsep(&dirs, ","))) {
    if (!strcmp(dir, "")) {
      dir = editor_working_directory(editor);
    } else if (!strcmp(dir , ".")) {
      char *path = editor->window->buffer->path;
      if (!path) {
        continue;
      }
      path = xstrdup(path);
      dir = dirname(path);
      free(path);
    }
    strcpy(buf, dir);
    strcat(buf, "/");
    strcat(buf, file);

    if (!access(buf, R_OK)) {
      char *result = xstrdup(buf);
      free(p);
      return result;
    }
  }
  free(p);
  return NULL;
}

char *editor_find_in_path(struct editor *editor, char *file) {
  char *path = editor_find_in_path_verbatim(editor, file);
  if (path) {
    return path;
  }
  char *suffixesadd = editor->window->buffer->opt.suffixesadd;
  if (!*suffixesadd) {
    return NULL;
  }

  char *p, *suffixes;
  p = suffixes = xstrdup(suffixesadd);

  char buf[PATH_MAX];

  char *suffix;
  while ((suffix = strsep(&suffixes, ","))) {
    strcpy(buf, file);
    strcat(buf, suffix);
    char *path = editor_find_in_path_verbatim(editor, buf);
    if (path) {
      free(p);
      return path;
    }
  }
  free(p);
  return NULL;
}

EDITOR_COMMAND(find, fin) {
  if (!arg) {
    editor_status_err(editor, "No file name");
    return;
  }

  char *path = editor_find_in_path(editor, arg);
  if (path) {
    editor_open(editor, path);
    free(path);
  } else {
    editor_status_err(editor, "Can't find file \"%s\" in path", arg);
  }
}

EDITOR_COMMAND(pwd, pw) {
  const char *pwd = editor_working_directory(editor);
  assert(pwd);
  editor_status_msg(editor, "%s", pwd);
}

EDITOR_COMMAND(chdir, cd) {
  const char *target = arg;
  if (!target) {
    target = homedir();
  }
  free(editor->pwd);
  editor->pwd = abspath(target);
  chdir(target);
  window_clear_working_directories(window_root(editor->window));
  editor_command_pwd(editor, NULL, false);
}

EDITOR_COMMAND(lchdir, lcd) {
  const char *target = arg;
  if (!target) {
    target = homedir();
  }

  if (!editor->pwd) {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    editor->pwd = xstrdup(cwd);
  }

  free(editor->window->pwd);
  editor->window->pwd = abspath(target);
  chdir(target);
  editor_command_pwd(editor, NULL, false);
}

void editor_set_window(struct editor *editor, struct window *window) {
  char *pwd = editor_working_directory(editor);

  if (window->pwd) {
    if (!pwd || strcmp(pwd, window->pwd)) {
      chdir(window->pwd);
    }
  } else if (editor->window->pwd) {
    assert(editor->pwd);
    chdir(editor->pwd);
  }
  editor->window = window;
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

EDITOR_COMMAND(source, so) {
  if (!arg) {
    editor_status_err(editor, "Argument required");
    return;
  }

  editor_source(editor, arg);
}

struct editor_command *command_parse(
    char *command, char **arg, bool *force) {
  if (!*command) {
    return NULL;
  }

  char *copy = xstrdup(command);
  char *name = strtok(copy, " ");
  size_t namelen = strlen(name);
  if (force) {
    *force = false;
  }
  if (name[namelen - 1] == '!') {
    if (force) {
      *force = true;
    }
    name[namelen - 1] = '\0';
  }
  if (arg) {
    *arg = strchr(command, ' ');
    if (*arg) {
      (*arg)++;
    }
  }
  struct editor_command *cmd;
  TAILQ_FOREACH(cmd, &editor_commands, pointers) {
    if (!strcmp(name, cmd->name) || !strcmp(name, cmd->shortname)) {
      free(copy);
      return cmd;
    }
  }
  free(copy);
  return NULL;
}

void editor_jump_to_line(struct editor *editor, int line) {
  struct gapbuf *gb = editor->window->buffer->text;
  line = max(line, 0);
  line = min(line, (int) gb_nlines(gb) - 1);

  window_set_cursor(editor->window, gb_linecol_to_pos(gb, line, 0));
}

void editor_jump_to_end(struct editor *editor) {
  struct gapbuf *gb = editor->window->buffer->text;
  editor_jump_to_line(editor, (int) gb_nlines(gb) - 1);
}

void editor_execute_command(struct editor *editor, char *command) {
  if (!*command) {
    return;
  }

  if (*command == '!') {
    bool has_subst = !!strchr(command + 1, '%');
    if (!editor->window->buffer->path && has_subst) {
      editor_status_err(editor, "Empty file name for '%%'");
      return;
    }

    if (has_subst) {
      command = strrep(command, "%", editor->window->buffer->path);
    }

    terminal_shutdown();

    printf("\n");

    system(command + 1);

    printf("\nPress ENTER to continue");
    fflush(stdout);

    char *line = NULL;
    size_t n = 0;
    getline(&line, &n, stdin);
    free(line);

    terminal_resume();

    if (has_subst) {
      free(command);
    }

    return;
  }

  bool force = false;
  char *arg = NULL;
  struct editor_command *cmd = command_parse(command, &arg, &force);
  if (cmd) {
    cmd->action(editor, arg, force);
    return;
  }

  size_t line, col;
  gb_pos_to_linecol(editor->window->buffer->text,
      window_cursor(editor->window), &line, &col);
  int target;
  if (strtoi(command, &target)) {
    target = labs(target);
    if (*command == '+') {
      target = (int) line + target;
    } else if (*command == '-') {
      target = (int) line - target;
    } else {
      target--;
    }
    editor_jump_to_line(editor, target);
    return;
  }

  editor_status_err(editor, "Not an editor command: %s", command);
}

static void editor_suspend(struct editor *editor) {
  terminal_suspend();
  editor_draw(editor);
}

static int editor_poll_event(struct editor *editor, struct tb_event *ev, bool *synthetic) {
  struct editor_event *top = TAILQ_FIRST(&editor->synthetic_events);
  if (top) {
    TAILQ_REMOVE(&editor->synthetic_events, top, pointers);
    memset(ev, 0, sizeof(*ev));
    ev->type = top->type;
    ev->key = top->key;
    ev->ch = top->ch;
    *synthetic = !top->from_test;
    free(top);
    return ev->type;
  }

  *synthetic = false;
  return tb_poll_event(ev);
}

bool editor_waitkey(struct editor *editor, struct tb_event *ev) {
  bool synthetic = false;
  if (editor_poll_event(editor, ev, &synthetic) < 0) {
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
    if (!synthetic && editor->recording) {
      struct editor_register *reg = editor_get_register(editor, editor->recording);
      assert(reg);
      char *rec = reg->read(reg);

      char ch = ev->ch;
      switch (ev->key) {
      case TB_KEY_ENTER: ch = '\n'; break;
      case TB_KEY_ESC: ch = '\e'; break;
      case TB_KEY_TAB: ch = '\t'; break;
      case TB_KEY_BACKSPACE: ch = '\b'; break;
      }
      char add[2] = {ch, '\0'};
      strcat(rec, add);
      reg->write(reg, rec);

      free(rec);
    }
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

void editor_push_event(struct editor *editor, struct tb_event *ev) {
  struct editor_event *event = xmalloc(sizeof(*event));
  memset(event, 0, sizeof(*event));
  event->type = ev->type;
  event->key = ev->key;
  event->ch = ev->ch;
  event->from_test = false;
  TAILQ_INSERT_HEAD(&editor->synthetic_events, event, pointers);
}

void editor_send_keys_internal(
    struct editor *editor, const char *keys, bool from_test) {
  struct editor_event *last = NULL;
  for (const char *k = keys; *k; ++k) {
    struct editor_event *ev = xmalloc(sizeof(*ev));
    ev->type = TB_EVENT_KEY;
    ev->key = 0;
    ev->ch = 0;
    ev->from_test = from_test;
    switch (*k) {
    case '<': {
      char key[10];
      size_t key_len = strcspn(k + 1, ">");
      strncpy(key, k + 1, key_len);
      key[key_len] = '\0';
      if (!strcmp("cr", key)) {
        ev->key = TB_KEY_ENTER;
      } else if (!strcmp("bs", key)) {
        ev->key = TB_KEY_BACKSPACE;
      } else if (!strcmp("esc", key)) {
        ev->key = TB_KEY_ESC;
      } else if (!strcmp("tab", key)) {
        ev->key = TB_KEY_TAB;
      } else if (!strcmp("up", key)) {
        ev->key = TB_KEY_ARROW_UP;
      } else if (!strcmp("down", key)) {
        ev->key = TB_KEY_ARROW_DOWN;
      } else if (!strcmp("C-w", key)) {
        ev->key = TB_KEY_CTRL_W;
      } else if (!strcmp("C-h", key)) {
        ev->key = TB_KEY_CTRL_H;
      } else if (!strcmp("C-l", key)) {
        ev->key = TB_KEY_CTRL_L;
      } else {
        debug("BUG: editor_send_keys got <%s>\n", key);
        exit(1);
      }
      k += key_len + 1;
      break;
    }
    case ' ': ev->key = TB_KEY_SPACE; break;
    case '\t': ev->key = TB_KEY_TAB; break;
    case '\n': ev->key = TB_KEY_ENTER; break;
    case '\e': ev->key = TB_KEY_ESC; break;
    case '\b': ev->key = TB_KEY_BACKSPACE; break;
    default:
      ev->ch = (uint32_t) *k;
      break;
    }
    // n.b. We want to correctly handle the case where editor_send_keys
    // indirectly recurses. For example 'o' is send_keys("A<cr>"), and 'A' is
    // send_keys("$i"). So we need to insert all the events in order, but
    // before any events that existed before we started this call.
    if (last) {
      TAILQ_INSERT_AFTER(&editor->synthetic_events, last, ev, pointers);
    } else {
      TAILQ_INSERT_HEAD(&editor->synthetic_events, ev, pointers);
    }
    last = ev;
  }

  struct tb_event ev;
  while (!TAILQ_EMPTY(&editor->synthetic_events)) {
    bool ok = editor_waitkey(editor, &ev);
    assert(ok);
    editor_handle_key_press(editor, &ev);
  }
}

void editor_send_keys(struct editor *editor, const char *keys) {
  editor_send_keys_internal(editor, keys, /* from_test */ false);
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

void editor_status_clear(struct editor *editor) {
  buf_clear(editor->status);
  if (editor->recording) {
    editor_status_msg(editor, "recording @%c", editor->recording);
  }
}

void editor_undo(struct editor *editor) {
  size_t cursor_pos;
  if (!buffer_undo(editor->window->buffer, &cursor_pos)) {
    editor_status_msg(editor, "Already at oldest change");
    return;
  }
  window_set_cursor(editor->window, cursor_pos);
}

void editor_redo(struct editor *editor) {
  size_t cursor_pos;
  if (!buffer_redo(editor->window->buffer, &cursor_pos)) {
    editor_status_msg(editor, "Already at newest change");
    return;
  }
  window_set_cursor(editor->window, cursor_pos);
}

bool editor_try_modify(struct editor *editor) {
  if (!editor->window->buffer->opt.modifiable) {
    editor_status_err(editor, "Cannot make changes, 'modifiable' is off");
    return false;
  }
  return true;
}

#define MODE(name) \
  void editor_push_##name##_mode(struct editor *editor, uint64_t arg) { \
    struct name##_mode *mode = &editor->modes.name; \
    mode->mode.arg = arg; \
    memset((char*)mode + sizeof(mode->mode), 0, sizeof(*mode) - sizeof(mode->mode)); \
    editor_push_mode(editor, &mode->mode); \
  } \
  struct name##_mode *editor_get_##name##_mode(struct editor *editor) { \
    assert(editor->mode->kind == EDITING_MODE_##name); \
    return (struct name##_mode*) editor->mode; \
  }
MODES
#undef MODE
