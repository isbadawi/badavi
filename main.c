#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "file.h"
#include "buf.h"

#include "util.h"

typedef struct {
  // The line the cursor is on.
  line_t *line;
  // The offset of the cursor within that line.
  int offset;
} cursor_t;

struct editing_mode_t;
typedef struct editing_mode_t editing_mode_t;

// The "editor" holds the main state of the program.
typedef struct {
  // The file being edited (only one file for now).
  file_t *file;
  // The path to the file being edited.
  char *path;
  // The top line visible on screen.
  line_t *top;
  // The leftmost column visible on screen.
  int left;
  // The editing mode (e.g. normal mode, insert mode, etc.)
  editing_mode_t* mode;
  // The cursor.
  cursor_t* cursor;
} editor_t;

// The editing mode for now is just a function that handles key events.
typedef void (mode_keypress_handler_t) (editor_t*, struct tb_event*);

struct editing_mode_t {
  mode_keypress_handler_t* key_pressed;
};

static void editor_ensure_cursor_visible(editor_t *editor) {
  int x = editor->cursor->offset - editor->left;
  // TODO(isbadawi): This might be expensive for files with many lines.
  int y = file_index_of_line(editor->file, editor->cursor->line) -
    file_index_of_line(editor->file, editor->top);

  if (x < 0) {
    editor->left += x;
  } else if (x >= tb_width()) {
    editor->left -= (tb_width() - x - 1);
  }

  while (y >= tb_height()) {
    editor->top = editor->top->next;
    y--;
  }

  while (y < 0) {
    editor->top = editor->top->prev;
    y++;
  }
}

// Draws the visible portion of the file to the screen.
void editor_draw(editor_t *editor) {
  tb_clear();

  editor_ensure_cursor_visible(editor);

  int y = 0;
  int w = tb_width();
  int h = tb_height();
  for (line_t *line = editor->top; line && y < h; line = line->next) {
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

  tb_present();
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

// The editor handles key presses by delegating to its mode.
void editor_handle_key_press(editor_t *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
  editor_draw(editor);
}

editing_mode_t normal_mode;
editing_mode_t insert_mode;

void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  if (ev->key & TB_KEY_ESC) {
    exit(0);
  }
  cursor_t *cursor = editor->cursor;
  switch (ev->ch) {
    case 'i':
      editor->mode = &insert_mode;
      break;
    case '0': cursor->offset = 0; break;
    case '$': cursor->offset = cursor->line->buf->len; break;
    case 'I':
      cursor->offset = 0;
      editor->mode = &insert_mode;
      break;
    case 'A':
      cursor->offset = cursor->line->buf->len;
      editor->mode = &insert_mode;
      break;
    case 'h': editor_move_left(editor); break;
    case 'l': editor_move_right(editor); break;
    case 'j': editor_move_down(editor); break;
    case 'k': editor_move_up(editor); break;
    case 'J':
      if (!cursor->line->next) {
        break;
      }
      buf_insert(cursor->line->buf, " ", cursor->line->buf->len);
      cursor->offset = cursor->line->buf->len;
      buf_insert(
          cursor->line->buf,
          cursor->line->next->buf->buf,
          cursor->line->buf->len);
      file_remove_line(editor->file, cursor->line->next);
      break;
    case 'x':
      if (cursor->offset == cursor->line->buf->len) {
        break;
      }
      buf_delete(cursor->line->buf, cursor->offset, 1);
      break;
    case 'D':
      buf_delete(
          cursor->line->buf,
          cursor->offset,
          cursor->line->buf->len - cursor->offset);
      break;
    case 'o':
      file_insert_line_after(editor->file, "", cursor->line);
      editor_move_down(editor);
      editor->mode = &insert_mode;
      break;
    case 'O':
      file_insert_line_after(editor->file, "", cursor->line->prev);
      cursor->line = cursor->line->prev;
      cursor->offset = 0;
      editor->mode = &insert_mode;
      break;
    // Just temporary until : commands are implemented
    case 's':
      file_write(editor->file, editor->path);
      break;
  }
}

void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  cursor_t *cursor = editor->cursor;
  char ch;
  switch (ev->key) {
    case TB_KEY_ESC:
      editor->mode = &normal_mode;
      return;
    case TB_KEY_BACKSPACE2:
      if (cursor->offset > 0) {
        buf_delete(cursor->line->buf, cursor->offset-- - 1, 1);
      } else if (cursor->line->prev->buf) {
        int prev_len = cursor->line->prev->buf->len;
        buf_insert(
            cursor->line->prev->buf,
            cursor->line->buf->buf,
            prev_len);
        editor_move_up(editor);
        file_remove_line(editor->file, cursor->line->next);
        cursor->offset = prev_len;
      }
      return;
    case TB_KEY_ENTER:
      file_insert_line_after(
          editor->file,
          cursor->line->buf->buf + cursor->offset,
          cursor->line);
      buf_delete(
          cursor->line->buf,
          cursor->offset,
          cursor->line->buf->len - cursor->offset);
      editor_move_down(editor);
      cursor->offset = 0;
      return;
    case TB_KEY_SPACE:
      ch = ' ';
      break;
    default:
      ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_insert(cursor->line->buf, s, cursor->offset++);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./badavi file\n");
    return -1;
  }

  debug_init();

  normal_mode.key_pressed = normal_mode_key_pressed;
  insert_mode.key_pressed = insert_mode_key_pressed;

  editor_t editor;
  editor.path = argv[1];
  editor.file = file_read(editor.path);
  editor.top = editor.file->head->next;
  editor.left = 0;
  editor.mode = &normal_mode;
  cursor_t cursor;
  cursor.line = editor.top;
  cursor.offset = 0;
  editor.cursor = &cursor;

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }
  atexit(tb_shutdown);

  editor_draw(&editor);

  struct tb_event ev;
  while (tb_poll_event(&ev)) {
    switch (ev.type) {
    case TB_EVENT_KEY:
      editor_handle_key_press(&editor, &ev);
      break;
    case TB_EVENT_RESIZE:
      editor_draw(&editor);
      break;
    default:
      break;
    }
  }

  return 0;
}
