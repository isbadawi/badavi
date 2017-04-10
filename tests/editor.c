#include "clar.h"
#include "editor.h"

#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "window.h"
#include "util.h"

static struct editor *editor = NULL;

static void type(const char *keys) {
  editor_send_keys(editor, keys);
}

static void assert_buffer_contents(const char *text) {
  struct buffer* buffer = editor->window->buffer;
  size_t expected_len = strlen(text);
  size_t actual_len  = gb_size(buffer->text);
  cl_assert_equal_i(expected_len, actual_len);
  struct buf *buf = gb_getstring(buffer->text, 0, expected_len);
  cl_assert_equal_s(buf->buf, text);
  buf_free(buf);
}

static void assert_cursor_at(
    size_t expected_line, size_t expected_column) {
  size_t actual_line, actual_column;
  gb_pos_to_linecol(
      editor->window->buffer->text,
      editor->window->cursor->start,
      &actual_line, &actual_column);
  cl_assert_equal_i(expected_line, actual_line);
  cl_assert_equal_i(expected_column, actual_column);
}

static void assert_cursor_over(char c) {
  struct gapbuf *text = editor->window->buffer->text;
  size_t cursor = editor->window->cursor->start;
  cl_assert_equal_i(gb_getchar(text, cursor), c);
}

void test_editor__initialize(void) {
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, 600, 600);
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
