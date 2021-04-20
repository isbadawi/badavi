#include "clar.h"
#include "window.h"

#include "buffer.h"

#define assert_window_whxy(window, w, h, x, y) do { \
  cl_assert_equal_i(window_w(window), w); \
  cl_assert_equal_i(window_h(window), h); \
  cl_assert_equal_i(window_x(window), x); \
  cl_assert_equal_i(window_y(window), y); \
} while (0)

struct buffer *buffer = NULL;
struct window *window = NULL;

void test_window__initialize(void) {
  buffer = buffer_create(NULL);
  window = window_create(buffer, 80, 16);
}

void test_window__cleanup(void) {
  window_free(window);
  buffer_free(buffer);
}

void test_window__create(void) {
  cl_assert(!window->parent);
  cl_assert_equal_i(window->split_type, WINDOW_LEAF);
  assert_window_whxy(window, 80, 16, 0, 0);

  cl_assert(!window_left(window));
  cl_assert(!window_right(window));
  cl_assert(!window_down(window));
  cl_assert(!window_up(window));
}

void test_window__vsplit(void) {
  struct window *left = window_split(window, WINDOW_SPLIT_LEFT);
  cl_assert_equal_p(left, window->split.first);
  struct window *right = window->split.second;

  cl_assert_equal_i(window->split_type, WINDOW_SPLIT_VERTICAL);
  cl_assert_equal_p(left->parent, window);
  cl_assert_equal_p(right->parent, window);

  cl_assert_equal_i(left->split_type, WINDOW_LEAF);
  assert_window_whxy(left, 40, 16, 0, 0);

  cl_assert_equal_i(right->split_type, WINDOW_LEAF);
  assert_window_whxy(right, 40, 16, 40, 0);

  cl_assert_equal_p(left, window_left(right));
  cl_assert_equal_p(right, window_right(left));
}

void test_window__split(void) {
  struct window *up = window_split(window, WINDOW_SPLIT_ABOVE);
  cl_assert_equal_p(up, window->split.first);
  struct window *down = window->split.second;

  cl_assert_equal_i(window->split_type, WINDOW_SPLIT_HORIZONTAL);
  cl_assert_equal_p(up->parent, window);
  cl_assert_equal_p(down->parent, window);

  cl_assert_equal_i(up->split_type, WINDOW_LEAF);
  assert_window_whxy(up, 80, 8, 0, 0);

  cl_assert_equal_i(down->split_type, WINDOW_LEAF);
  assert_window_whxy(down, 80, 8, 0, 8);

  cl_assert_equal_p(up, window_up(down));
  cl_assert_equal_p(down, window_down(up));
}

void test_window__close(void) {
  window_split(window, WINDOW_SPLIT_ABOVE);

  struct window *old_window = window->split.second;
  struct window *next = window_close(window->split.second);
  cl_assert(!next->parent);
  cl_assert_equal_i(next->split_type, WINDOW_LEAF);
  assert_window_whxy(next, 80, 16, 0, 0);
  window_free(old_window);
}

void test_window__resize(void) {
  // _________
  // |   |   |
  // | A |   |
  // |---| C |
  // | B |   |
  // |___|___|
  struct window *left = window_split(window, WINDOW_SPLIT_LEFT);
  struct window *A = window_split(left, WINDOW_SPLIT_ABOVE);
  struct window *B = left->split.second;
  struct window *C = window->split.second;

  cl_assert_equal_p(B, window_down(A));
  cl_assert_equal_p(A, window_up(B));
  cl_assert_equal_p(C, window_right(A));
  cl_assert_equal_p(C, window_right(B));

  assert_window_whxy(A, 40, 8, 0, 0);
  assert_window_whxy(B, 40, 8, 0, 8);
  assert_window_whxy(C, 40, 16, 40, 0);

  window_resize(A, 0, 5);
  assert_window_whxy(A, 40, 13, 0, 0);
  assert_window_whxy(B, 40, 3, 0, 13);
  assert_window_whxy(C, 40, 16, 40, 0);

  window_resize(B, 0, 10);
  assert_window_whxy(A, 40, 3, 0, 0);
  assert_window_whxy(B, 40, 13, 0, 3);
  assert_window_whxy(C, 40, 16, 40, 0);

  window_resize(A, 20, 0);
  assert_window_whxy(A, 60, 3, 0, 0);
  assert_window_whxy(B, 60, 13, 0, 3);
  assert_window_whxy(C, 20, 16, 60, 0);

  window_resize(C, 40, 0);
  assert_window_whxy(A, 20, 3, 0, 0);
  assert_window_whxy(B, 20, 13, 0, 3);
  assert_window_whxy(C, 60, 16, 20, 0);

  window_equalize(window, WINDOW_SPLIT_HORIZONTAL);
  assert_window_whxy(A, 20, 8, 0, 0);
  assert_window_whxy(B, 20, 8, 0, 8);
  assert_window_whxy(C, 60, 16, 20, 0);

  window_equalize(window, WINDOW_SPLIT_VERTICAL);
  assert_window_whxy(A, 40, 8, 0, 0);
  assert_window_whxy(B, 40, 8, 0, 8);
  assert_window_whxy(C, 40, 16, 40, 0);
}
