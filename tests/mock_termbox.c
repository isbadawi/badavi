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

void tb_clear(void) {
  return;
}

void tb_present(void) {
  return;
}

void tb_change_cell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg) {
  struct tb_cell *cell = &cell_buffer[y * MOCK_TB_WIDTH + x];
  cell->ch = ch;
  cell->fg = fg;
  cell->bg = bg;
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

