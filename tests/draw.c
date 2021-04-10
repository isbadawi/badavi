#include "clar.h"
#include "editor.h"

#include <string.h>
#include <termbox.h>

static struct editor *editor = NULL;
static struct tb_cell *cells = NULL;
char line_buffer[80];

static void type(const char *keys) {
  editor_send_keys(editor, keys);
}

static char color(uint32_t color) {
  switch (color) {
    case TB_DEFAULT: return '.';
    case TB_BLACK: return 'b';
    case TB_RED: return 'r';
    case TB_YELLOW: return 'y';
    case TB_BLUE: return 'b';
    case 0x07: return 'w';
    case TB_DARK_GRAY: return 'g';
    default: return '?';
  }
}

static char chars(struct tb_cell *cell) { return cell->ch ? cell->ch : '.'; }
static char fgcolors(struct tb_cell *cell) { return color(cell->fg); }
static char bgcolors(struct tb_cell *cell) { return color(cell->bg); }

#define assert_line(number, func, expected) do { \
  struct tb_cell *line = &cells[tb_width() * number]; \
  size_t len = strlen(expected); \
  size_t i; \
  for (i = 0; i < len; ++i) { \
    line_buffer[i] = func(&line[i]); \
  } \
  line_buffer[i] = '\0'; \
  cl_assert_equal_s(expected, line_buffer); \
} while (0)

void test_draw__initialize(void) {
  tb_init();
  cells = tb_cell_buffer();
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, tb_width(), tb_height());
}

void test_draw__simple_text(void) {
  type("ihello, world!");
  editor_draw(editor);
  assert_line(0, chars,    "hello, world!");
  assert_line(0, fgcolors, "wwwwwwwwwwwww");
  assert_line(0, bgcolors, ".............");
}

void test_draw__visual_mode(void) {
  type("ihello, world!<esc>0lvw");
  editor_draw(editor);
  assert_line(0, chars,    "hello, world!");
  assert_line(0, fgcolors, "wwwwwbwwwwwww");
  assert_line(0, bgcolors, ".ggggw.......");
}

void test_draw__linewise_visual_mode(void) {
  type("ihello, world!<esc>0lVw");
  editor_draw(editor);
  assert_line(0, chars,    "hello, world!");
  assert_line(0, fgcolors, "wwwwwbwwwwwww");
  assert_line(0, bgcolors, "gggggwggggggg");
}

void test_draw__number(void) {
  type(":set number<cr>");
  type("ihello, world!");
  editor_draw(editor);
  assert_line(0, chars,    "  1.hello, world!");
  assert_line(0, fgcolors, "yyy.wwwwwwwwwwwww");
  assert_line(0, bgcolors, ".................");
}

void test_draw__hlsearch(void) {
  type(":set hlsearch<cr>");
  type("ihello, word world!<esc>0");
  type("/wor<cr>");
  editor_draw(editor);
  assert_line(0, chars,    "hello, word world!");
  assert_line(0, fgcolors, "wwwwwwwbbbwwbbbwww");
  assert_line(0, bgcolors, ".......wyy..yyy...");
}
