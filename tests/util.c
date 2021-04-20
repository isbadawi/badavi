#include "clar.h"
#include "util.h"

#define cl_assert_equal_s_free(a, b) { \
  char *_tmp = a; \
  cl_assert_equal_s(_tmp, b); \
  free(_tmp); \
}

void test_util__abspath(void) {
  cl_assert_equal_s_free(abspath("/a//b"), "/a/b");
  cl_assert_equal_s_free(abspath("/a/./b"), "/a/b");
  cl_assert_equal_s_free(abspath("/a//./b"), "/a/b");
  cl_assert_equal_s_free(abspath("/"), "/");
  cl_assert_equal_s_free(abspath("/.."), "/");
  cl_assert_equal_s_free(abspath("/../a"), "/a");
  cl_assert_equal_s_free(abspath("/.././..//../.././"), "/");
  cl_assert_equal_s_free(abspath("/a/../b"), "/b");
  cl_assert_equal_s_free(abspath("/a//b/../c/./d"), "/a/c/d");
  cl_assert_equal_s_free(abspath("/a/b/c/d/../../e"), "/a/b/e");
  cl_assert_equal_s_free(abspath("/a/b/c/d/../../e/../.."), "/a");
  cl_assert_equal_s_free(abspath("/a/./b/c//d/.././../e/../../f"), "/a/f");
  cl_assert_equal_s_free(abspath("/a/.."), "/");
}

void test_util__relpath(void) {
  cl_assert_equal_s(relpath("/a/b", "/a/b"), "");
  cl_assert_equal_s(relpath("/a/b", "/a"), "b");
  cl_assert_equal_s(relpath("/foo.txt", "/a/b"), "/foo.txt");
}

void test_util__strrep(void) {
  cl_assert_equal_s_free(strrep("hello %", "%", "world"), "hello world");
  cl_assert_equal_s_free(strrep("foo % % bar", "%", "baz"), "foo baz baz bar");
  cl_assert_equal_s_free(strrep("barb barn", "bar", "foo"), "foob foon");
}
