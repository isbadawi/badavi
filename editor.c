#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <termbox.h>

#include "buf.h"
#include "file.h"
#include "mode.h"
#include "util.h"

void editor_init(editor_t *editor, cursor_t *cursor, char *path) {
  editor->status = buf_create(tb_width() / 2);
  editor->path = path;

  if (!editor->path) {
    editor->file = file_create();
  } else if (access(editor->path, F_OK) < 0) {
    editor->file = file_create();
    buf_printf(editor->status, "\"%s\" [New File]", editor->path);
  } else {
    editor->file = file_open(editor->path);
    buf_printf(editor->status, "\"%s\" %dL, %dC",
        editor->path, editor->file->nlines, file_size(editor->file));
  }

  editor->top = editor->file->head->next;
  editor->left = 0;
  editor->mode = &normal_mode;
  editor->cursor = cursor;

  cursor->line = editor->top;
  cursor->offset = 0;
}

void editor_save_file(editor_t *editor, char *path) {
  if (path) {
    file_write(editor->file, path);
    buf_printf(editor->status, "\"%s\" %dL, %dC written",
        path, editor->file->nlines, file_size(editor->file));
    if (!editor->path) {
      editor->path = path;
    }
  } else {
    buf_printf(editor->status, "No file name");
  }
}

void editor_execute_command(editor_t *editor, char *command) {
  char *cmd = strtok(command, " ");
  char *arg = strtok(NULL, " ");
  if (!strcmp(cmd, "q")) {
    // TODO(isbadawi): Error if file has unsaved changes.
    exit(0);
  } else if (!strcmp(cmd, "w")) {
    char *path = arg ? arg : editor->path;
    editor_save_file(editor, path);
  } else if (!strcmp(cmd, "wq")) {
    editor_save_file(editor, editor->path);
    exit(0);
  } else {
    buf_printf(editor->status, "Not an editor command: %s", command);
  }
}

static void editor_ensure_cursor_visible(editor_t *editor) {
  int w = tb_width();
  int h = tb_height();
  int x = editor->cursor->offset - editor->left;
  // TODO(isbadawi): This might be expensive for files with many lines.
  int y = file_index_of_line(editor->file, editor->cursor->line) -
    file_index_of_line(editor->file, editor->top);

  if (x < 0) {
    editor->left += x;
  } else if (x >= w) {
    editor->left -= w - x - 1;
  }

  while (y >= h - 1) {
    editor->top = editor->top->next;
    y--;
  }

  while (y < 0) {
    editor->top = editor->top->prev;
    y++;
  }
}

void editor_draw(editor_t *editor) {
  tb_clear();

  editor_ensure_cursor_visible(editor);

  int y = 0;
  int w = tb_width();
  int h = tb_height();
  for (line_t *line = editor->top; line && y < h - 1; line = line->next) {
    int x = 0;
    for (int i = editor->left; i < line->buf->len && x < w; ++i) {
      tb_change_cell(x++, y, line->buf->buf[i], TB_WHITE, TB_DEFAULT);
    }
    if (line == editor->cursor->line) {
      int x = editor->cursor->offset;
      tb_change_cell(x - editor->left, y, line->buf->buf[x], TB_DEFAULT, TB_WHITE);
    }
    y++;
  }

  for (int x = 0; x < editor->status->len; ++x) {
    tb_change_cell(x, h - 1, editor->status->buf[x], TB_WHITE, TB_DEFAULT);
  }

  tb_present();
}

// The editor handles key presses by delegating to its mode.
void editor_handle_key_press(editor_t *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
  editor_draw(editor);
}

void editor_move_left(editor_t *editor) {
  editor->cursor->offset = max(editor->cursor->offset - 1, 0);
}

void editor_move_right(editor_t *editor) {
  int len = editor->cursor->line->buf->len;
  editor->cursor->offset = min(editor->cursor->offset + 1, len);
}

void editor_move_up(editor_t *editor) {
  cursor_t *cursor = editor->cursor;
  if (!cursor->line->prev->buf) {
    return;
  }
  cursor->line = cursor->line->prev;
  cursor->offset = min(cursor->offset, cursor->line->buf->len);
}

void editor_move_down(editor_t *editor) {
  cursor_t *cursor = editor->cursor;
  if (!cursor->line->next) {
    return;
  }
  cursor->line = cursor->line->next;
  cursor->offset = min(cursor->offset, cursor->line->buf->len);
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
