#include "clar.h"
#include "editor.h"

#include <stddef.h>
#include <string.h>
#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "window.h"
#include "util.h"

#include "asserts.h"

static struct editor *editor = NULL;

static char *type(const char *keys) {
  editor_send_keys(editor, keys);
  return editor->status->buf;
}

void test_editor__initialize(void) {
  tb_init();
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, tb_width(), tb_height());
  assert_buffer_contents("\n");
  assert_cursor_at(0, 0);
}

void test_editor__basic_editing(void) {
  type("i");
  type("hello");
  assert_cursor_at(0, 5);
  assert_buffer_contents("hello\n");
  type("<bs><bs><bs>");
  assert_cursor_at(0, 2);
  assert_buffer_contents("he\n");
  type("<esc>hi");
  type("iv");
  assert_cursor_at(0, 3);
  assert_buffer_contents("hive\n");
}

void test_editor__basic_motions(void) {
  type("ithe quick brown fox jumps over the lazy dog<esc>0");
  assert_cursor_over('t');

  type("w");  assert_cursor_over('q');
  type("e");  assert_cursor_over('k');
  type("ge"); assert_cursor_over('e');
  type("b");  assert_cursor_over('t');
  type("tx"); assert_cursor_over('o');
  type("fr"); assert_cursor_over('r');
  type("Tn"); assert_cursor_over(' ');
  type("Fh"); assert_cursor_over('h');
  type("4l"); assert_cursor_over('u');
  type("$b"); assert_cursor_over('d');
}

void test_editor__jump_to_line(void) {
  type("ihello<cr>hello<cr>hello<cr>hello<cr>hello<esc>gg");
  assert_cursor_at(0, 0);
  type(":2<cr>");
  assert_cursor_at(1, 0);
  type(":-1<cr>");
  assert_cursor_at(0, 0);
  type(":+3<cr>");
  assert_cursor_at(3, 0);

  type(":-20<cr>");
  assert_cursor_at(0, 0);
  type(":+100<cr>");
  assert_cursor_at(4, 0);
}

void test_editor__line_motions(void) {
  type("ione\ntwo\nthree\nfour\nfive<esc>gg0");
  assert_cursor_at(0, 0);

  type("k"); assert_cursor_at(0, 0);
  type("j"); assert_cursor_at(1, 0);
  type("2j"); assert_cursor_at(3, 0);
  type("gg"); assert_cursor_at(0, 0);
  type("G"); assert_cursor_at(4, 0);
  type("3G"); assert_cursor_at(2, 0);

  type("gg0/our<cr>"); assert_cursor_at(3, 1);
  type("G0?wo<cr>"); assert_cursor_at(1, 1);
}

void test_editor__yank_put(void) {
  assert_buffer_contents("\n");
  assert_cursor_at(0, 0);

  type("ithe quick brown fox jumps over the lazy dog<esc>0");

  // 'brown' in "a register
  type("2w\"ayw");
  // 'fox' in "b register
  type("w\"byw");
  // whole line in unnamed register
  type("0d$");

  type("\"bp\"ap");
  type("o<esc>p");

  assert_buffer_contents(
      "fox brown \n"
      "the quick brown fox jumps over the lazy dog\n"
  );
}

void test_editor__join_lines(void) {
  type("ihello\n     world<esc>gg");
  type("J");
  assert_buffer_contents("hello world\n");
  assert_cursor_at(0, 5);
}

void test_editor__percent_motion(void) {
  type("i{ ( [ ) ] [ ] }<esc>0");
  assert_cursor_over('{');
  type("%"); assert_cursor_over('}');
  type("%"); assert_cursor_over('{');
  type("l%"); assert_cursor_over(')');
  type("%"); assert_cursor_over('(');
  type("l%"); assert_cursor_over(']'); assert_cursor_at(0, 8);
  type("l%"); assert_cursor_over(']'); assert_cursor_at(0, 12);
  type("%"); assert_cursor_over('['); assert_cursor_at(0, 10);
}

void test_editor__command_history_traversal(void) {
  type(":set number<cr>");
  type(":set norelativenumber<cr>");

  type(":");

  cl_assert_equal_s(type("<up>"), ":set norelativenumber");
  cl_assert_equal_s(type("<up>"), ":set number");
  cl_assert_equal_s(type("<up>"), ":set number");
  cl_assert_equal_s(type("<down>"), ":set norelativenumber");
  cl_assert_equal_s(type("<down>"), ":");
  cl_assert_equal_s(type("<down>"), ":");
}

void test_editor__command_history_filter_prefix(void) {
  type(":set number<cr>");
  type(":set norelativenumber<cr>");
  type(":nohlsearch<cr>");
  type(":set nomodifiable<cr>");
  type(":vsplit<cr>");

  type(":set no");

  cl_assert_equal_s(type("<up>"), ":set nomodifiable");
  cl_assert_equal_s(type("<up>"), ":set norelativenumber");
  cl_assert_equal_s(type("<up>"), ":set norelativenumber");
  cl_assert_equal_s(type("<down>"), ":set nomodifiable");
  cl_assert_equal_s(type("<down>"), ":set no");
}

void test_editor__command_history_duplicates_move_to_front(void) {
  type(":nohlsearch<cr>");
  type(":set nomodifiable<cr>");
  type(":vsplit<cr>");
  type(":set nomodifiable<cr>");

  type(":");

  cl_assert_equal_s(type("<up>"), ":set nomodifiable");
  cl_assert_equal_s(type("<up>"), ":vsplit");
  cl_assert_equal_s(type("<up>"), ":nohlsearch");
  cl_assert_equal_s(type("<up>"), ":nohlsearch");
}

void test_editor__autoindent(void) {
  type(":set autoindent<cr>");
  type("i    hello<cr>world");
  assert_buffer_contents("    hello\n    world\n");

  type("<esc>ggdG");
  type(":set smartindent<cr>");
  type(":set shiftwidth=2<cr>");

  type("ihello {<cr>world");
  assert_buffer_contents("hello {\n  world\n");

  type("<esc>ggdG");
  type(":set cinwords=custom<cr>");
  type("icustom<cr>indented");
  assert_buffer_contents("custom\n  indented\n");
}
