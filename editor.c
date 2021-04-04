#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include <libclipboard.h>
#include <termbox.h>

#include "attrs.h"
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

void register_editor_command(struct editor_command *command) {
  if (!commands_init_done) {
    TAILQ_INIT(&editor_commands);
    commands_init_done = true;
  }
  TAILQ_INSERT_TAIL(&editor_commands, command, pointers);
}

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
  editor->width = width;
  editor->height = height;

  editor->status = buf_create(editor->width);
  editor->status_error = false;
  editor->status_silence = false;
  editor->status_cursor = 0;

  editor->highlight_search_matches = false;

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

  TAILQ_INIT(&editor->buffers);
  struct buffer *buffer = buffer_create(NULL);
  buffer_inherit_editor_options(buffer, editor);
  TAILQ_INSERT_TAIL(&editor->buffers, buffer, pointers);

  editor->window = window_create(buffer, editor->width, editor->height - 1);

  history_init(&editor->command_history, &editor->opt.history);
  history_init(&editor->search_history, &editor->opt.history);
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

  if (path) {
    free(path);
  }
  return rc;
}

EDITOR_COMMAND(qall, qa) {
  if (!force) {
    struct buffer *buffer;
    TAILQ_FOREACH(buffer, &editor->buffers, pointers) {
      if (buffer->opt.modified) {
        const char *path = buffer->path;
        if (path) {
          path = editor_relpath(editor, path);
        } else {
          path = "[No Name]";
        }
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

EDITOR_COMMAND(write, w) {
  if (!force && editor->window->buffer->opt.readonly) {
    editor_status_err(editor, "'readonly' option is set (add ! to override)");
    return;
  }

  editor->window->buffer->opt.readonly = false;
  editor_save_buffer(editor, arg);
}

EDITOR_COMMAND(edit, e) {
  if (arg) {
    editor_open(editor, arg);
  } else {
    editor_status_err(editor, "No file name");
  }
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
  if (editor->pwd) {
    free(editor->pwd);
  }
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

  if (editor->window->pwd) {
    free(editor->window->pwd);
  }
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

  char *copy = xstrdup(command);
  char *name = strtok(command, " ");
  size_t namelen = strlen(name);
  bool force = false;
  if (name[namelen - 1] == '!') {
    force = true;
    name[namelen - 1] = '\0';
  }
  char *arg = strtok(NULL, " ");
  struct editor_command *cmd;
  TAILQ_FOREACH(cmd, &editor_commands, pointers) {
    if (!strcmp(name, cmd->name) || !strcmp(name, cmd->shortname)) {
      cmd->action(editor, arg, force);
      free(copy);
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
  struct editor_event *top = TAILQ_FIRST(&editor->synthetic_events);
  if (top) {
    TAILQ_REMOVE(&editor->synthetic_events, top, pointers);
    memset(ev, 0, sizeof(*ev));
    ev->type = top->type;
    ev->key = top->key;
    ev->ch = top->ch;
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
  struct editor_event *last = NULL;
  for (const char *k = keys; *k; ++k) {
    struct editor_event *ev = xmalloc(sizeof(*ev));
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
