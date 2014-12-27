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
  {'"', NULL},
  {-1, NULL}
};

void editor_init(editor_t *editor) {
  editor->status = buf_create(tb_width() / 2);
  editor->status_error = 0;
  editor->buffers = buffer_create(NULL);
  editor->windows = window_create(NULL);
  editor->window = NULL;
  editor->mode = normal_mode();

  editor->registers = register_table;
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    editor->registers[i].buf = buf_create(1);
  }

  editor->count = 0;
  editor->motion = NULL;
}

buf_t *editor_get_register(editor_t *editor, char name) {
  for (int i = 0; editor->registers[i].name != -1; ++i) {
    if (editor->registers[i].name == name) {
      return editor->registers[i].buf;
    }
  }
  return NULL;
}

static void editor_add_buffer(editor_t *editor, buffer_t *buffer) {
  buffer_t *b = editor->buffers;
  for (; b->next; b = b->next);
  b->next = buffer;
}

static void editor_add_window(editor_t *editor, window_t *window) {
  window_t *w = editor->windows;
  for (; w->next; w = w->next);
  w->next = window;
}

static window_t *editor_get_window_by_name(editor_t* editor, char *name) {
  for (window_t *w = editor->windows->next; w; w = w->next) {
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
    editor_add_buffer(editor, buffer);
    window = window_create(buffer);
    editor_add_window(editor, window);
  }
  editor->window = window;
}

void editor_open_empty(editor_t *editor) {
  buffer_t *buffer = buffer_create(NULL);
  editor_add_buffer(editor, buffer);
  window_t *window = window_create(buffer);
  editor_add_window(editor, window);
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
  char *cmd = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  if (!strcmp(cmd, "q")) {
    for (buffer_t *b = editor->buffers->next; b != NULL; b = b->next) {
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
