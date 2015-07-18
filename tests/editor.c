#include "clar.h"
#include "editor.h"

#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "window.h"
#include "util.h"

static struct editor_t *editor = NULL;

void test_editor__initialize(void) {
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, 600, 600);
}

static void type(const char *keys) {
  editor_send_keys(editor, keys);
}

static void assert_buffer_contents(const char *text) {
  struct buffer_t* buffer = editor->window->buffer;
  size_t expected_len = strlen(text);
  size_t actual_len  = gb_size(buffer->text);
  cl_assert_equal_i(expected_len, actual_len);
  struct buf_t *buf = buf_create(actual_len);
  gb_getstring(buffer->text, 0, expected_len, buf->buf);
  cl_assert_equal_s(buf->buf, text);
}

static void assert_cursor_position(size_t pos) {
  cl_assert_equal_i(editor->window->cursor->start, pos);
}

static void assert_cursor_over(char c) {
  struct gapbuf_t *text = editor->window->buffer->text;
  size_t cursor = editor->window->cursor->start;
  cl_assert_equal_i(gb_getchar(text, cursor), c);
}

void test_editor__basic_editing(void) {
  assert_buffer_contents("\n");
  assert_cursor_position(0);
  type("i");
  type("hello");
  assert_cursor_position(5);
  assert_buffer_contents("hello\n");
  type("<bs><bs><bs>");
  assert_cursor_position(2);
  assert_buffer_contents("he\n");
  type("<esc>hi");
  type("iv");
  assert_cursor_position(3);
  assert_buffer_contents("hive\n");
}

void test_editor__basic_motions(void) {
  assert_buffer_contents("\n");
  assert_cursor_position(0);
  type("i");
  type("the quick brown fox jumps over the lazy dog");
  type("<esc>0");
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
