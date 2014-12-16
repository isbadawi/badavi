#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "piece.h"

#define max(x, y) ((x) > (y) ? (x) : (y))

struct cursor {
  int x;
  int y;
};

struct editor {
  struct mode* mode;
  piece_table_t* piece_table;
  struct cursor cursor;
};

typedef void (mode_keypress_handler_t) (struct editor*, struct tb_event*);

struct mode {
  mode_keypress_handler_t* key_pressed;
};

void editor_draw(struct editor*);
void display_file(piece_table_t*);
void draw_cursor(struct cursor);

void editor_draw(struct editor *editor) {
  tb_clear();
  display_file(editor->piece_table);
  draw_cursor(editor->cursor);
  tb_present();
}

void editor_handle_key_press(struct editor *editor, struct tb_event *ev) {
  editor->mode->key_pressed(editor, ev);
}

void normal_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  if (ev->key & TB_KEY_ESC) {
    exit(0);
  }
  switch (ev->ch) {
    case 'h':
      editor->cursor.x = max(editor->cursor.x - 1, 0);
      break;
    case 'j':
      editor->cursor.y++;
      break;
    case 'k':
      editor->cursor.y = max(editor->cursor.y - 1, 0);
      break;
    case 'l':
      editor->cursor.x++;
      break;
  }
  editor_draw(editor);
}

struct mode normal_mode = {normal_mode_key_pressed};

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

void draw_cursor(struct cursor cursor) {
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

  struct editor editor;
  editor.mode = &normal_mode;
  editor.piece_table = piece_table_new(argv[1]);
  editor.cursor.x = editor.cursor.y = 0;

  piece_table_dump(editor.piece_table);

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
