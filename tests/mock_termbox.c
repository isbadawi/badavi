#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "attrs.h"

#define MOCK_TB_WIDTH 80
#define MOCK_TB_HEIGHT 24

static struct tb_cell cell_buffer[MOCK_TB_WIDTH * MOCK_TB_HEIGHT];

int tb_init(void) {
  memset(cell_buffer, 0, sizeof(cell_buffer));
  return 0;
}

void tb_shutdown(void) {
  return;
}

int tb_width(void) {
  return MOCK_TB_WIDTH;
}

int tb_height(void) {
  return MOCK_TB_HEIGHT;
}

void tb_clear_buffer(void) {
  for (int i = 0; i < MOCK_TB_HEIGHT; ++i) {
    for (int j = 0; j < MOCK_TB_WIDTH; ++j) {
      tb_char(j, i, TB_DEFAULT, TB_DEFAULT, '\0');
    }
  }
}

void tb_render(void) {
  return;
}

void tb_char(int x, int y, tb_color fg, tb_color bg, tb_chr ch) {
  struct tb_cell *cell = &cell_buffer[y * MOCK_TB_WIDTH + x];
  cell->ch = ch;
  cell->fg = fg;
  cell->bg = bg;
}

int tb_string(int x, int y, tb_color fg, tb_color bg, const char * str) {
  int l;
  for (l = 0; *str; l++) {
    tb_char(x++, y, fg, bg, *str++);
  }
  return l;
}

int tb_string_with_limit(int x, int y, tb_color fg, tb_color bg,
                         const char * str, int limit) {
  int l;
  for (l = 0; *str && l < limit; l++) {
    tb_char(x++, y, fg, bg, *str++);
  }
  return l;
}

int tb_stringf(int x, int y, tb_color fg, tb_color bg, const char *fmt, ...) {
  char buf[512];
  va_list vl;
  va_start(vl, fmt);
  vsnprintf(buf, sizeof(buf), fmt, vl);
  va_end(vl);
  return tb_string(x, y, fg, bg, buf);
}

void tb_empty(int x, int y, tb_color bg, int width) {
  char buf[512];
  snprintf(buf, sizeof(buf), "%*s", width, "");
  tb_string(x, y, TB_DEFAULT, bg, buf);
}

struct tb_cell *tb_cell_buffer(void) {
  return cell_buffer;
}

int tb_select_output_mode(int mode ATTR_UNUSED) {
  return 0;
}


int tb_poll_event(struct tb_event *event ATTR_UNUSED) {
  return 0;
}

int tb_utf8_char_length(char c ATTR_UNUSED) {
  return 1;
}

int tb_utf8_char_to_unicode(uint32_t *out, const char *c) {
  *out = (uint32_t) *c;
  return 1;
}

int tb_utf8_unicode_to_char(char *out, uint32_t c) {
  *out = (char) c;
  return 1;
}
