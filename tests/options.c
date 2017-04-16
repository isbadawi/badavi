#include "clar.h"
#include "editor.h"

#include "buf.h"
#include "util.h"

static struct editor *editor = NULL;

static char *type(const char *keys) {
  editor_send_keys(editor, keys);
  return editor->status->buf;
}

void test_options__initialize(void) {
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, 600, 600);
}

void test_options__buffer_local(void) {
  // 'modifiable' by default is on.
  cl_assert_equal_s(type(":set  modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setl modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setg modifiable?<cr>"), "modifiable");

  // Turn it off locally for this buffer.
  type(":setl nomodifiable<cr>");

  // It's off for this buffer, but the global one is on.
  cl_assert_equal_s(type(":set  modifiable?<cr>"), "nomodifiable");
  cl_assert_equal_s(type(":setl modifiable?<cr>"), "nomodifiable");
  cl_assert_equal_s(type(":setg modifiable?<cr>"), "modifiable");

  // Create a new buffer; it inherits the global value.
  type(":e newbuffer.c<cr>");
  cl_assert_equal_s(type(":set  modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setl modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setg modifiable?<cr>"), "modifiable");

  // Turn it off globally.
  type(":setg nomodifiable<cr>");

  // Turning it off globally doesn't affect existing buffers.
  cl_assert_equal_s(type(":set  modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setl modifiable?<cr>"), "modifiable");
  cl_assert_equal_s(type(":setg modifiable?<cr>"), "nomodifiable");

  // New buffer inherits the global value, which is now off.
  type(":e newbuffer2.c<cr>");
  cl_assert_equal_s(type(":set  modifiable?<cr>"), "nomodifiable");
  cl_assert_equal_s(type(":setl modifiable?<cr>"), "nomodifiable");
  cl_assert_equal_s(type(":setg modifiable?<cr>"), "nomodifiable");
}

void test_options__window_local(void) {
  // 'number' by default is off.
  cl_assert_equal_s(type(":set number?<cr>"), "nonumber");

  // Turn it on.
  type(":set number<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "number");

  // Split windows; the new window inherits the value.
  type(":split<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "number");

  // Turn it off in this window.
  type(":set nonumber<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "nonumber");

  // It doesn't affect the value in the original window.
  type(":q<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "number");
}
