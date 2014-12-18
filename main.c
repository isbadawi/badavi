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

  // y offset from the top of the screen.
  // Used to implement scrolling -- not sure if this belongs here.
  int y;
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

// Draws the visible portion of the file to the screen.
void editor_draw(editor_t *editor) {
  tb_clear();

  int y = 0;
  int w = tb_width();
  int h = tb_height();
  for (line_t *line = editor->top; line && y < h; line = line->next) {
    debug("y = %d\n", y);
    for (int x = 0; x < line->buf->len && x < w; ++x) {
      tb_change_cell(x, y, line->buf->buf[x], TB_WHITE, TB_DEFAULT);
    }
    if (line == editor->cursor->line) {
      int x = editor->cursor->offset;
      tb_change_cell(x, y, line->buf->buf[x], TB_DEFAULT, TB_WHITE);
    }
    y++;
  }

  tb_present();
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
    // TODO(isbadawi): Scrolling left and right
    case 'h':
      cursor->offset = max(cursor->offset - 1, 0);
      break;
    case 'l':
      cursor->offset = min(cursor->offset + 1, cursor->line->buf->len);
      break;
    case 'j':
      if (!cursor->line->next) {
        break;
      }
      cursor->line = cursor->line->next;
      cursor->offset = min(cursor->offset, cursor->line->buf->len);

      if (cursor->y == tb_height() - 1) {
        editor->top = editor->top->next;
      } else {
        cursor->y++;
      }
      break;
    case 'k':
      if (!cursor->line->prev->buf) {
        break;
      }
      cursor->line = cursor->line->prev;
      cursor->offset = min(cursor->offset, cursor->line->buf->len);

      if (cursor->y == 0) {
        editor->top = editor->top->prev;
      } else {
        cursor->y--;
      }
      break;
    case 'x':
      if (cursor->offset == cursor->line->buf->len) {
        break;
      }
      buf_delete(cursor->line->buf, cursor->offset, 1);
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
      // TODO(isbadawi): Handle backspace (and find out why BACKSPACE2)
      return;
    case TB_KEY_ENTER:
      ch = '\n';
      break;
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
  editor.mode = &normal_mode;
  cursor_t cursor;
  cursor.line = editor.top;
  cursor.offset = 0;
  cursor.y = 0;
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
