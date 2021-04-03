#include "clar.h"
#include "util.h"

void test_util__abspath(void) {
  cl_assert_equal_s(abspath("/a//b"), "/a/b");
  cl_assert_equal_s(abspath("/a/./b"), "/a/b");
  cl_assert_equal_s(abspath("/a//./b"), "/a/b");
  cl_assert_equal_s(abspath("/"), "/");
  cl_assert_equal_s(abspath("/.."), "/");
  cl_assert_equal_s(abspath("/../a"), "/a");
  cl_assert_equal_s(abspath("/.././..//../.././"), "/");
  cl_assert_equal_s(abspath("/a/../b"), "/b");
  cl_assert_equal_s(abspath("/a//b/../c/./d"), "/a/c/d");
  cl_assert_equal_s(abspath("/a/b/c/d/../../e"), "/a/b/e");
  cl_assert_equal_s(abspath("/a/b/c/d/../../e/../.."), "/a");
  cl_assert_equal_s(abspath("/a/./b/c//d/.././../e/../../f"), "/a/f");
  cl_assert_equal_s(abspath("/a/.."), "/");
}

void test_util__relpath(void) {
  cl_assert_equal_s(relpath("/a/b", "/a/b"), "");
  cl_assert_equal_s(relpath("/a/b", "/a"), "b");
  cl_assert_equal_s(relpath("/foo.txt", "/a/b"), "/foo.txt");
}

void test_util__strrep(void) {
  cl_assert_equal_s(strrep("hello %", "%", "world"), "hello world");
  cl_assert_equal_s(strrep("foo % % bar", "%", "baz"), "foo baz baz bar");
  cl_assert_equal_s(strrep("barb barn", "bar", "foo"), "foob foon");
}
