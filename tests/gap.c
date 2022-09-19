#include "clar.h"
#include "gap.h"

#include <stddef.h>
#include <string.h>

#include "buf.h"
#include "util.h"

#include "asserts.h"

#define assert_line(pos, text) do { \
  struct buf *line = gb_getline(gb, pos); \
  cl_assert_equal_s(text, line->buf); \
  buf_free(line); \
} while (0);

void test_gap__getline(void) {
  struct buf *str = buf_from_cstr("hello\nworld\n");
  struct gapbuf *gb = gb_fromstring(str);

  assert_line(0, "hello");
  assert_line(5, "hello");
  assert_line(6, "world");

  buf_free(str);
  gb_free(gb);
}

void test_gap__getlines(void) {
  struct buf *str = buf_from_cstr("hello\nworld\n");
  struct gapbuf *gb = gb_fromstring(str);

  struct region *lines = gb_getlines(gb);

  char line[256];
  gb_getstring_into(gb, lines[0].start, lines[0].end - lines[0].start, line);
  cl_assert_equal_s("hello", line);

  gb_getstring_into(gb, lines[1].start, lines[1].end - lines[1].start, line);
  cl_assert_equal_s("world", line);

  free(lines);
  buf_free(str);
  gb_free(gb);
}
