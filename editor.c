#include "editor.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "mode.h"
#include "util.h"

static editor_register_t register_table[] = {
  // Last search pattern register
  // TODO(isbadawi): Investigate segfaults when this register is listed later.
  {'/'},
  // Unnamed default register
  {'"'},
  // Named registers
  {'a'}, {'b'}, {'c'}, {'d'}, {'e'}, {'f'}, {'g'}, {'h'}, {'i'}, {'j'}, {'k'},
  {'l'}, {'m'}, {'n'}, {'o'}, {'p'}, {'q'}, {'r'}, {'s'}, {'t'}, {'u'}, {'v'},
  {'w'}, {'x'}, {'y'}, {'z'},
  // Sentinel
  {-1}
};

void editor_init(editor_t *editor) {
  editor->status = buf_create(tb_width() / 2);
  editor->status_error = 0;
  editor->buffers = list_create();
  list_append(editor->buffers, buffer_create(NULL));
  editor->windows = list_create();
  list_append(editor->windows, window_create(NULL, 0, 0, 0, 0));
  editor->window = NULL;
  editor->mode = normal_mode();

  editor->registers = register_table;
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    editor->registers[i].buf = buf_create(1);
  }

  editor->count = 0;
  editor->motion = NULL;
  editor->register_ = '"';
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
  int *cursor = &editor->window->cursor;

  char pattern[SEARCH_PATTERN_MAXLEN];
  memcpy(pattern, reg->buf, reg->len);
  pattern[reg->len] = '\0';

  int offset = gb_search_forwards(gb, pattern, *cursor + 1);
  if (offset == -2) {
    editor_status_err(editor, "Bad pattern: \"%s\"", pattern);
    return;
  }
  if (offset == -1) {
    editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
    offset = gb_search_forwards(gb, pattern, 0);
  }
  if (offset == -1) {
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
  } else {
    *cursor = offset;
  }
}

static window_t *editor_get_window_by_name(editor_t* editor, char *name) {
  window_t *w;
  LIST_FOREACH(editor->windows, w) {
    if (w->buffer && w->buffer->name && !strcmp(w->buffer->name, name)) {
      return w;
    }
  }
  return NULL;
}

void editor_open(editor_t *editor, char *path) {
  window_t *window = editor_get_window_by_name(editor, path);

  if (!window) {
    buffer_t *buffer;
    if (access(path, F_OK) < 0) {
      buffer = buffer_create(path);
      editor_status_msg(editor, "\"%s\" [New File]", path);
    } else {
      buffer = buffer_open(path);
      editor_status_msg(editor, "\"%s\" %dL, %dC",
          path, gb_nlines(buffer->text), gb_size(buffer->text));
    }
    list_append(editor->buffers, buffer);
    window = window_create(buffer, 0, 0, tb_width(), tb_height() - 1);
    list_append(editor->windows, window);
  }
  editor->window = window;
}

void editor_open_empty(editor_t *editor) {
  buffer_t *buffer = buffer_create(NULL);
  list_append(editor->buffers, buffer);
  window_t *window = window_create(buffer, 0, 0, tb_width(), tb_height() - 1);
  list_append(editor->windows, window);
  editor->window = window;
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
    editor_status_msg(editor, "\"%s\" %dL, %dC written",
        name, gb_nlines(buffer->text), gb_size(buffer->text));
  } else {
    editor_status_err(editor, "No file name");
  }
}

void editor_execute_command(editor_t *editor, char *command) {
  if (!*command) {
    return;
  }
  char *cmd = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  if (!strcmp(cmd, "q")) {
    buffer_t *b;
    LIST_FOREACH(editor->buffers, b) {
      if (b->dirty) {
        editor_status_err(editor,
            "No write since last change for buffer \"%s\"",
            b->name ? b->name : "[No Name]");
        return;
      }
    }
    exit(0);
  } else if (!strcmp(cmd, "q!")) {
    exit(0);
  } else if (!strcmp(cmd, "w")) {
    editor_save_buffer(editor, arg);
  } else if (!strcmp(cmd, "wq")) {
    editor_save_buffer(editor, NULL);
    exit(0);
  } else if (!strcmp(cmd, "e")) {
    if (arg) {
      editor_open(editor, arg);
    } else {
      editor_status_err(editor, "No file name");
    }
  } else {
    editor_status_err(editor, "Not an editor command: %s", command);
  }
}

void editor_draw(editor_t *editor) {
  tb_clear();

  // TODO(isbadawi): Multiple windows at the same time.
  window_draw(editor->window);

  int nlines = gb_nlines(editor->window->buffer->text);
  for (int y = nlines; y < tb_height() - 1; ++y) {
    tb_change_cell(0, y, '~', TB_BLUE, TB_DEFAULT);
  }

  for (int x = 0; x < editor->status->len; ++x) {
    tb_change_cell(x, tb_height() - 1, editor->status->buf[x],
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
  int len = strlen(keys);
  struct tb_event ev;
  for (int i = 0; i < len; ++i) {
    ev.key = 0;
    switch (keys[i]) {
    case '<': {
      char key[10];
      int key_len = strcspn(keys + i + 1, ">");
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
      ev.ch = keys[i];
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
  editor->status_error = 0;
}

void editor_status_err(editor_t *editor, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(editor->status, format, args);
  va_end(args);
  editor->status_error = 1;
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
    }
    list_clear(group);
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
