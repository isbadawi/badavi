#pragma once

#define assert_contents(text_) do { \
  size_t expected_len = strlen(text_); \
  size_t actual_len  = gb_size(buffer->text); \
  cl_assert_equal_i(expected_len, actual_len); \
  struct buf *buf = gb_getstring(buffer->text, 0, expected_len); \
  cl_assert_equal_s(buf->buf, text_); \
  buf_free(buf); \
} while (0)

#define assert_buffer_contents(text_) do { \
  struct buffer* buffer = editor->window->buffer; \
  assert_contents(text_); \
} while (0)

#define assert_cursor_at(expected_line, expected_column) do { \
  size_t actual_line, actual_column; \
  gb_pos_to_linecol( \
      editor->window->buffer->text, \
      window_cursor(editor->window), \
      &actual_line, &actual_column); \
  cl_assert_equal_i(expected_line, actual_line); \
  cl_assert_equal_i(expected_column, actual_column); \
} while (0)

#define assert_cursor_over(c) do { \
  struct gapbuf *text = editor->window->buffer->text; \
  size_t cursor = window_cursor(editor->window); \
  cl_assert_equal_i(gb_getchar(text, cursor), c); \
} while (0)

