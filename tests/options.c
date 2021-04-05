#include "clar.h"
#include "editor.h"

#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "window.h"

#include "util.h"

static struct editor *editor = NULL;

static char *type(const char *keys) {
  editor_send_keys(editor, keys);
  return editor->status->buf;
}

void test_options__initialize(void) {
  tb_init();
  editor = xmalloc(sizeof(*editor));
  editor_init(editor, tb_width(), tb_height());
}

void test_options__option_types(void) {
  type(":set ruler<cr>");
  cl_assert(editor->opt.ruler);
  cl_assert_equal_s(type(":set ruler?<cr>"), "ruler");

  type(":set numberwidth=6<cr>");
  cl_assert_equal_i(editor->window->opt.numberwidth, 6);
  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=6");

  type(":set cinwords=hello,world<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=hello,world");
  cl_assert_equal_s(editor->opt.cinwords, "hello,world");
  cl_assert_equal_s(editor->window->buffer->opt.cinwords, "hello,world");
}

void test_options__reset_to_default(void) {
  cl_assert_equal_s(type(":set number?<cr>"), "nonumber");
  type(":set number<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "number");
  type(":set number&<cr>");
  cl_assert_equal_s(type(":set number?<cr>"), "nonumber");

  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=4");
  type(":set numberwidth=24<cr>");
  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=24");
  type(":set numberwidth&<cr>");
  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=4");

  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=if,else,while,do,for,switch");
  type(":set cinwords=foo,bar<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=foo,bar");
  type(":set cinwords&<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=if,else,while,do,for,switch");
}

void test_options__plus_equals(void) {
  type(":set numberwidth=4<cr>");
  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=4");
  type(":set numberwidth+=6<cr>");
  cl_assert_equal_s(type(":set numberwidth?<cr>"), "numberwidth=10");

  type(":set cinwords=<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=");
  type(":set cinwords+=foo<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=foo");
  type(":set cinwords+=bar<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=foo,bar");
  type(":set cinwords+=foo<cr>");
  cl_assert_equal_s(type(":set cinwords?<cr>"), "cinwords=foo,bar");
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
