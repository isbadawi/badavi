#include "clar.h"
#include "editor.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <termbox.h>

#include <limits.h>
#include <unistd.h>

#include "buf.h"
#include "buffer.h"
#include "gap.h"
#include "window.h"
#include "util.h"

#include "asserts.h"

static struct editor *editor = NULL;

static char *type(const char *keys) {
  editor_send_keys(editor, keys);
  return editor->status->buf;
}

char oldcwd[PATH_MAX] = {0};
char scratch[PATH_MAX] = {0};

void test_os__initialize(void) {
  if (!*oldcwd) {
    getcwd(oldcwd, sizeof(oldcwd));
    strcpy(scratch, oldcwd);
    strcat(scratch, "/scratch");
  }

  // Note that clar runs tests inside a tmp dir, so relative paths are ok.
  system("rm -rf ./scratch && mkdir -p ./scratch");
  chdir(scratch);
  system(
      "echo 'hello world' > foo.txt && "
      "echo 'foo bar' > bar.txt && "
      "mkdir -p subdir && echo 'magic' > subdir/baz.txt");

  editor = xmalloc(sizeof(*editor));
  editor_init(editor, tb_width(), tb_height());
}

void test_os__cleanup(void) {
  chdir(oldcwd);
}

void test_os__open_file(void) {
  type(":e foo.txt<cr>");
  assert_buffer_contents("hello world\n");
}

void test_os__chdir_commands(void) {
  cl_assert_equal_s(type(":pwd<cr>"), scratch);
  cl_assert_equal_s(type(":cd ..<cr>:pwd<cr>"), oldcwd);
  cl_assert_equal_s(type(":cd scratch<cr>:pwd<cr>"), scratch);

  type(":set splitright<cr>:vsplit<cr>");
  cl_assert_equal_s(type(":lcd ..<cr>:pwd<cr>"), oldcwd);
  cl_assert_equal_s(type("<C-w><C-h>:pwd<cr>"), scratch);
  cl_assert_equal_s(type("<C-w><C-l>:pwd<cr>"), oldcwd);
}

void test_os__browse_directory(void) {
  type(":e .<cr>");
  assert_buffer_contents("bar.txt\nfoo.txt\nsubdir/\n");

  type("jj<cr>");

  assert_buffer_contents("baz.txt\n");
  type("<cr>");
  assert_buffer_contents("magic\n");
  type("--");
  assert_buffer_contents(
      "bar.txt\n"
      "foo.txt\n"
      "subdir/\n");
  assert_cursor_at(2, 0);
}

void test_os__write_files(void) {
  type(":e foo.txt<cr>");
  assert_buffer_contents("hello world\n");
  type(":w copy.txt<cr>");
  type(":e copy.txt<cr>");
  assert_buffer_contents("hello world\n");
  type(":e .<cr>");
  assert_buffer_contents("bar.txt\ncopy.txt\nfoo.txt\nsubdir/\n");
}

void test_os__shell_command(void) {
  // Suppress the "Press ENTER to continue" prompt
  int fd = dup(fileno(stdout));
  freopen("/dev/null", "w", stdout);
  ungetc('\n', stdin);

  type(":!rm bar.txt<cr><cr>");

  // Restore stdout back to what it was
  dup2(fd, fileno(stdout));
  close(fd);

  type(":e .<cr>");
  assert_buffer_contents("foo.txt\nsubdir/\n");
}
