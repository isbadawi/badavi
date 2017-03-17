#include "clar.h"
#include "window.h"

#include "buffer.h"

static void assert_window_whxy(
    struct window *window, size_t w, size_t h, size_t x, size_t y) {
  cl_assert_equal_i(window_w(window), w);
  cl_assert_equal_i(window_h(window), h);
  cl_assert_equal_i(window_x(window), x);
  cl_assert_equal_i(window_y(window), y);
}

void test_window__create(void) {
  struct window *window = window_create(buffer_create(NULL), 80, 16);

  cl_assert(!window->parent);
  cl_assert_equal_i(window->split_type, WINDOW_LEAF);
  assert_window_whxy(window, 80, 16, 0, 0);

  cl_assert(!window_left(window));
  cl_assert(!window_right(window));
  cl_assert(!window_down(window));
  cl_assert(!window_up(window));
}

void test_window__vsplit(void) {
  struct window *window = window_create(buffer_create(NULL), 80, 16);

  struct window *left = window_split(window, WINDOW_SPLIT_VERTICAL);
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
  struct window *window = window_create(buffer_create(NULL), 80, 16);

  struct window *up = window_split(window, WINDOW_SPLIT_HORIZONTAL);
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
  struct window *window = window_create(buffer_create(NULL), 80, 16);

  window_split(window, WINDOW_SPLIT_HORIZONTAL);

  struct window *next = window_close(window->split.second);
  cl_assert(!next->parent);
  cl_assert_equal_i(next->split_type, WINDOW_LEAF);
  assert_window_whxy(next, 80, 16, 0, 0);
}

void test_window__resize(void) {
  // _________
  // |   |   |
  // | A |   |
  // |---| C |
  // | B |   |
  // |___|___|
  struct window *root = window_create(buffer_create(NULL), 80, 16);
  struct window *left = window_split(root, WINDOW_SPLIT_VERTICAL);
  struct window *A = window_split(left, WINDOW_SPLIT_HORIZONTAL);
  struct window *B = left->split.second;
  struct window *C = root->split.second;

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

  window_equalize(root, WINDOW_SPLIT_HORIZONTAL);
  assert_window_whxy(A, 20, 8, 0, 0);
  assert_window_whxy(B, 20, 8, 0, 8);
  assert_window_whxy(C, 60, 16, 20, 0);

  window_equalize(root, WINDOW_SPLIT_VERTICAL);
  assert_window_whxy(A, 40, 8, 0, 0);
  assert_window_whxy(B, 40, 8, 0, 8);
  assert_window_whxy(C, 40, 16, 40, 0);
}
