#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "util.h"

struct cursor {
  int x;
  int y;
};

struct editor {
  struct mode* mode;
  char *buffer;
  int nlines;
  struct cursor cursor;
};

typedef void (mode_keypress_handler_t) (struct editor*, struct tb_event*);

struct mode {
  mode_keypress_handler_t* key_pressed;
};

void editor_draw(struct editor*);
void display_file(char *, int);
void draw_cursor(struct cursor);

void editor_draw(struct editor *editor) {
  tb_clear();
  display_file(editor->buffer, editor->nlines);
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
      editor->cursor.y = min(editor->cursor.y + 1, editor->nlines - 1);
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


void draw_text(int x, int y, const char* text) {
  int len = strlen(text);
  for (int i = 0; i < len; ++i) {
    tb_change_cell(x + i, y, text[i], TB_WHITE, TB_DEFAULT);
  }
}

void display_file(char *contents, int nlines) {
  char *line = contents;
  for (int i = 0; i < nlines; ++i) {
    draw_text(0, i, line);
    line = line + strlen(line) + 1;
  }
}

void draw_cursor(struct cursor cursor) {
  struct tb_cell *cells = tb_cell_buffer();
  struct tb_cell *cell = &cells[cursor.y * tb_width() + cursor.x];
  uint16_t temp = cell->fg;
  cell->fg = cell->bg;
  cell->bg = temp;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./badavi file\n");
    return -1;
  }

  struct editor editor;
  editor.mode = &normal_mode;
  editor.buffer = read_file(argv[1]);
  editor.nlines = split_lines(editor.buffer);
  editor.cursor.x = editor.cursor.y = 0;

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
