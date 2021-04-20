#include "clar.h"
#include "buffer.h"

#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "gap.h"
#include "util.h"

#include "asserts.h"

static struct buffer *buffer = NULL;

static void insert_text(size_t pos, char *text) {
  buffer_do_insert(buffer, buf_from_cstr(text), pos);
}

static void delete_text(size_t pos, size_t len) {
  buffer_do_delete(buffer, len, pos);
}

void test_buffer__initialize(void) {
  buffer = buffer_create(NULL);
}

void test_buffer__cleanup(void) {
  buffer_free(buffer);
}

void test_buffer__empty(void) {
  cl_assert_equal_p(buffer->path, NULL);
  cl_assert(!buffer->opt.modified);
  assert_contents("\n");
}

void test_buffer__insert_delete(void) {
  insert_text(0, "hello, world");
  assert_contents("hello, world\n");

  delete_text(1, 3);
  assert_contents("ho, world\n");

  cl_assert(buffer->opt.modified);
}

void test_buffer__undo_redo(void) {
  size_t cursor_pos;

  assert_contents("\n");

  buffer_start_action_group(buffer);
  insert_text(0, "world");
  assert_contents("world\n");

  buffer_start_action_group(buffer);
  insert_text(0, "hello, ");
  assert_contents("hello, world\n");

  buffer_undo(buffer, &cursor_pos);
  assert_contents("world\n");
  cl_assert_equal_i(cursor_pos, 0);

  buffer_undo(buffer, &cursor_pos);
  assert_contents("\n");
  cl_assert_equal_i(cursor_pos, 0);

  buffer_redo(buffer, &cursor_pos);
  assert_contents("world\n");
  cl_assert_equal_i(cursor_pos, 0);

  buffer_redo(buffer, &cursor_pos);
  assert_contents("hello, world\n");
  cl_assert_equal_i(cursor_pos, 0);

  buffer_start_action_group(buffer);
  delete_text(1, 3);
  assert_contents("ho, world\n");

  buffer_undo(buffer, &cursor_pos);
  assert_contents("hello, world\n");
  cl_assert_equal_i(cursor_pos, 1);
}

void test_buffer__undo_group(void) {
  buffer_start_action_group(buffer);
  size_t cursor_pos;

  assert_contents("\n");

  insert_text(0, "world");
  assert_contents("world\n");
  insert_text(0, "hello, ");
  assert_contents("hello, world\n");

  buffer_undo(buffer, &cursor_pos);
  assert_contents("\n");
  cl_assert_equal_i(cursor_pos, 0);

  buffer_redo(buffer, &cursor_pos);
  assert_contents("hello, world\n");
  cl_assert_equal_i(cursor_pos, 0);
}

void test_buffer__marks(void) {
  struct mark mark;
  region_set(&mark.region, 0, 1);
  TAILQ_INSERT_TAIL(&buffer->marks, &mark, pointers);

  cl_assert_equal_i(mark.region.start, 0);
  cl_assert_equal_i(mark.region.end, 1);

  insert_text(0, "hello, world");

  cl_assert_equal_i(mark.region.start, 12);
  cl_assert_equal_i(mark.region.end, 13);
}
