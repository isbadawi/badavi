#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "piece.h"

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
  editing_mode_t* mode;
  piece_table_t* piece_table;
  cursor_t cursor;
} editor_t;

typedef void (mode_keypress_handler_t) (editor_t*, struct tb_event*);

struct editing_mode_t {
  mode_keypress_handler_t* key_pressed;
};

void editor_draw(editor_t*);
void display_file(piece_table_t*);
void draw_cursor(cursor_t);

void editor_draw(editor_t *editor) {
  tb_clear();
  display_file(editor->piece_table);
  draw_cursor(editor->cursor);
  tb_present();
}

void editor_handle_key_press(editor_t *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
}

void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  if (ev->key & TB_KEY_ESC) {
    exit(0);
  }
  cursor_t *cursor = &editor->cursor;
  switch (ev->ch) {
    case 'h':
      if (cursor->x > 0) {
        cursor->x--;
        cursor->offset--;
      }
      break;
    // TODO(isbadawi): Keep track of offset across vertical moves.
    case 'j':
      cursor->y++;
      break;
    case 'k':
      cursor->y = max(cursor->y - 1, 0);
      break;
    case 'l':
      cursor->x++;
      cursor->offset++;
      break;
    case 'x':
      piece_table_delete(editor->piece_table, cursor->offset);
      break;
    // Just for debugging
    case 'm':
      piece_table_dump(editor->piece_table, DEBUG_FP);
      break;
  }
  editor_draw(editor);
}

editing_mode_t normal_mode = {normal_mode_key_pressed};

void display_file(piece_table_t *table) {
  int x = 0;
  int y = 0;
  for (int i = 0; i < table->size; ++i) {
    char c = piece_table_get(table, i);
    if (c == '\n') {
      ++y;
      x = 0;
    } else {
      tb_change_cell(x++, y, c, TB_WHITE, TB_DEFAULT);
    }
  }
}

void draw_cursor(cursor_t cursor) {
  struct tb_cell *cells = tb_cell_buffer();
  struct tb_cell *cell = &cells[cursor.y * tb_width() + cursor.x];
  cell->bg = TB_WHITE;
  cell->fg = TB_DEFAULT;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./badavi file\n");
    return -1;
  }

  DEBUG_FP = fopen("log.txt", "w");

  editor_t editor;
  editor.mode = &normal_mode;
  editor.piece_table = piece_table_new(argv[1]);
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
