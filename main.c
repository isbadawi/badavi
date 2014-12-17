#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "piece.h"

#define debug(...) fprintf(DEBUG_FP, __VA_ARGS__); fflush(DEBUG_FP);
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

static FILE *DEBUG_FP;

typedef struct {
  // x and y are the screen coordinates.
  int x; int y;
  // offset is the offset from the start of the file.
  int offset;
} cursor_t;

struct editing_mode_t;
typedef struct editing_mode_t editing_mode_t;

typedef struct {
  char *file;
  int scroll_offset;
  editing_mode_t* mode;
  piece_table_t* piece_table;
  cursor_t cursor;
} editor_t;

typedef void (mode_keypress_handler_t) (editor_t*, struct tb_event*);

struct editing_mode_t {
  mode_keypress_handler_t* key_pressed;
};

void editor_draw(editor_t *editor) {
  tb_clear();

  int x = 0;
  int y = 0;

  int h = tb_height();
  int len = editor->piece_table->size;
  for (int i = editor->scroll_offset; y != h && i < len; ++i) {
    char c = piece_table_get(editor->piece_table, i);
    if (c == '\n') {
      ++y;
      x = 0;
    } else {
      tb_change_cell(x++, y, c, TB_WHITE, TB_DEFAULT);
    }
  }

  struct tb_cell *cells = tb_cell_buffer();
  int cell_index = editor->cursor.y * tb_width() + editor->cursor.x;
  struct tb_cell *cell = &cells[cell_index];
  cell->bg = TB_WHITE;
  cell->fg = TB_DEFAULT;

  tb_present();
}

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
  cursor_t *cursor = &editor->cursor;
  switch (ev->ch) {
    case 'i':
      editor->mode = &insert_mode;
      break;
    case 'h':
      if (cursor->x > 0) {
        cursor->x--;
        cursor->offset--;
      }
      break;
    case 'j': {
      int next_line = piece_table_index_of(
          editor->piece_table, '\n', cursor->offset);
      int next_next_line = piece_table_index_of(
          editor->piece_table, '\n', next_line + 1);
      if (next_next_line == -1) {
        break;
      }
      int next_line_length = next_next_line - next_line - 1;
      cursor->y++;
      cursor->x = min(cursor->x, max(0, next_line_length - 1));
      cursor->offset = next_line + cursor->x + 1;

      if (cursor->y == tb_height()) {
        int next_top_line = piece_table_index_of(
            editor->piece_table, '\n', editor->scroll_offset);
        if (next_top_line != -1) {
          editor->scroll_offset = 1 + next_top_line;
          cursor->y--;
        }
      }
      break;
    }
    case 'k': {
      int prev_line = piece_table_last_index_of(
          editor->piece_table, '\n', cursor->offset - 1);
      if (prev_line == -1) {
        break;
      }
      int prev_prev_line = piece_table_last_index_of(
          editor->piece_table, '\n', prev_line - 1);
      int prev_line_length = prev_line - prev_prev_line - 1;
      cursor->y--;
      cursor->x = min(cursor->x, max(0, prev_line_length - 1));
      cursor->offset = prev_prev_line + cursor->x + 1;

      if (cursor->y == -1) {
        int prev_top_line = piece_table_last_index_of(
            editor->piece_table, '\n', editor->scroll_offset - 2);
        editor->scroll_offset = 1 + prev_top_line;
        cursor->y++;
      }
      break;
    }
    case 'l': {
      int next_line = piece_table_index_of(
          editor->piece_table, '\n', cursor->offset);
      if (cursor->offset == next_line ||
          cursor->offset + 1 == next_line) {
        break;
      }
      cursor->x++;
      cursor->offset++;
      break;
    }
    case 'x':
      piece_table_delete(editor->piece_table, cursor->offset);
      break;
    // Just for debugging
    case 'm':
      piece_table_dump(editor->piece_table, DEBUG_FP);
      break;
    case 's':
      piece_table_write(editor->piece_table, editor->file);
      break;
  }
}

void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  cursor_t *cursor = &editor->cursor;
  char ch;
  switch (ev->key) {
    case TB_KEY_ESC:
      editor->mode = &normal_mode;
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
  piece_table_insert(editor->piece_table, cursor->offset++, ch);
  if (ch == '\n') {
    cursor->x = 0;
    cursor->y++;
  } else {
    cursor->x++;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./badavi file\n");
    return -1;
  }

  DEBUG_FP = fopen("log.txt", "w");

  normal_mode.key_pressed = normal_mode_key_pressed;
  insert_mode.key_pressed = insert_mode_key_pressed;

  editor_t editor;
  editor.file = argv[1];
  editor.scroll_offset = 0;
  editor.mode = &normal_mode;
  editor.piece_table = piece_table_new(editor.file);
  memset(&editor.cursor, 0, sizeof(cursor_t));

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
