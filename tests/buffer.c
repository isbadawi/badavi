#include "clar.h"
#include "buffer.h"

#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "gap.h"
#include "list.h"
#include "util.h"

static struct buffer_t *buffer = NULL;

static void assert_contents(const char *text) {
  size_t expected_len = strlen(text);
  size_t actual_len  = gb_size(buffer->text);
  cl_assert_equal_i(expected_len, actual_len);
  struct buf_t *buf = buf_create(actual_len);
  gb_getstring(buffer->text, 0, expected_len, buf->buf);
  cl_assert_equal_s(buf->buf, text);
}

static void insert_text(size_t pos, char *text) {
  buffer_do_insert(buffer, buf_from_cstr(text), pos);
}

void test_buffer__empty(void) {
  buffer = buffer_create(NULL);
  cl_assert_equal_s(buffer->name, "");
  cl_assert(!buffer->dirty);
  assert_contents("\n");
}

void test_buffer__insert(void) {
  buffer = buffer_create(NULL);
  insert_text(0, "hello, world");
  assert_contents("hello, world\n");
  cl_assert(buffer->dirty);
}

void test_buffer__undo_redo(void) {
  buffer = buffer_create(NULL);

  assert_contents("\n");

  buffer_start_action_group(buffer);
  insert_text(0, "world");
  assert_contents("world\n");

  buffer_start_action_group(buffer);
  insert_text(0, "hello, ");
  assert_contents("hello, world\n");

  buffer_undo(buffer);
  assert_contents("world\n");
  buffer_undo(buffer);
  assert_contents("\n");

  buffer_redo(buffer);
  assert_contents("world\n");
  buffer_redo(buffer);
  assert_contents("hello, world\n");
}

void test_buffer__undo_group(void) {
  buffer = buffer_create(NULL);
  buffer_start_action_group(buffer);

  assert_contents("\n");

  insert_text(0, "world");
  assert_contents("world\n");
  insert_text(0, "hello, ");
  assert_contents("hello, world\n");

  buffer_undo(buffer);
  assert_contents("\n");
  buffer_redo(buffer);
  assert_contents("hello, world\n");
}

void test_buffer__marks(void) {
  buffer = buffer_create(NULL);
  struct region_t mark = {0, 1};
  list_append(buffer->marks, &mark);

  cl_assert_equal_i(mark.start, 0);
  cl_assert_equal_i(mark.end, 1);

  insert_text(0, "hello, world");

  cl_assert_equal_i(mark.start, 12);
  cl_assert_equal_i(mark.end, 13);
}
