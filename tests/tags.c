#include "clar.h"
#include "tags.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "editor.h"
#include "util.h"
#include "window.h"

#include "asserts.h"

static struct editor *editor = NULL;
static struct tags *tags = NULL;

static char *type(const char *keys) {
  editor_send_keys(editor, keys);
  return editor->status->buf;
}

void test_tags__initialize(void) {
  cl_fixture_sandbox("tags.c");
  cl_fixture_sandbox("tags");
  tb_init();
  editor = editor_create(tb_width(), tb_height());
  tags = editor->tags;
  cl_assert(tags && tags->len > 0);
}

void test_tags__cleanup(void) {
  editor_free(editor);
  cl_fixture_cleanup("tags");
}

void test_tags__find(void) {
  struct tag *tag = tags_find(tags, "foo");
  cl_assert(tag);
  cl_assert_equal_s("foo", tag->name);
  cl_assert_equal_s("tags.c", tag->path);
  cl_assert_equal_s("/^void foo\\(void\\) \\{$", tag->cmd);

  struct tag *garbage = tags_find(tags, "garbage");
  cl_assert(!garbage);
}

void test_tags__updated(void) {
  time_t first_loaded = --tags->loaded_at;
  utimes(tags->file, NULL);
  tags_find(tags, "foo");
  cl_assert(tags->loaded_at > first_loaded);
}

void test_tags__deleted(void) {
  cl_assert(tags->len > 0);
  remove(tags->file);
  struct tag *tag = tags_find(tags, "foo");
  cl_assert(!tag);
  cl_assert_equal_i(tags->len, 0);
}

#define assert_editor_error(msg) do { \
  cl_assert_equal_i(editor->status_error, true); \
  cl_assert_equal_s(editor->status->buf, msg); \
} while (0)

void test_tags__editor_tag_stack(void) {
  editor_jump_to_tag(editor, "garbage");
  assert_editor_error("tag not found: garbage");

  editor_tag_stack_prev(editor);
  assert_editor_error("tag stack empty");
  editor_tag_stack_next(editor);
  assert_editor_error("tag stack empty");

  // Build up the tag stack
  assert_cursor_at(0, 0);
  editor_jump_to_tag(editor, "foo");
  assert_cursor_at(9, 0);
  editor_jump_to_tag(editor, "bar");
  assert_cursor_at(6, 0);
  editor_jump_to_tag(editor, "baz");
  assert_cursor_at(3, 0);

  // Navigate up and down
  editor_tag_stack_next(editor);
  assert_editor_error("at top of tag stack");

  editor_tag_stack_prev(editor);
  assert_cursor_at(6, 0);
  editor_tag_stack_next(editor);
  assert_cursor_at(3, 0);

  editor_tag_stack_prev(editor);
  editor_tag_stack_prev(editor);
  editor_tag_stack_prev(editor);
  assert_cursor_at(0, 0);

  editor_tag_stack_prev(editor);
  assert_editor_error("at bottom of tag stack");

  // Jump to a new tag from within the tag stack
  editor_tag_stack_next(editor);
  editor_tag_stack_next(editor);
  assert_cursor_at(6, 0);
  editor_jump_to_tag(editor, "quux");
  assert_cursor_at(1, 0);

  // 'baz' (3, 0) no longer on the stack
  editor_tag_stack_prev(editor);
  assert_cursor_at(6, 0);
  editor_tag_stack_next(editor);
  assert_cursor_at(1, 0);
  editor_tag_stack_next(editor);
  assert_editor_error("at top of tag stack");
}

void test_tags__completion(void) {
  cl_assert_equal_s(type(":tag "), ":tag ");
  cl_assert_equal_s(type("<tab>"), ":tag bar");
  cl_assert_equal_s(type("<tab>"), ":tag baz");
  cl_assert_equal_s(type("<tab>"), ":tag foo");
  cl_assert_equal_s(type("<tab>"), ":tag quux");
  cl_assert_equal_s(type("<tab>"), ":tag bar");
  cl_assert_equal_s(type("<esc>:tag f<tab>"), ":tag foo");
  type("<esc>");
}
