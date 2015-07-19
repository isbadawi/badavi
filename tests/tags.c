#include "clar.h"
#include "tags.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static struct tags_t *tags = NULL;

void test_tags__initialize(void) {
  cl_fixture_sandbox("tags");
  tags = tags_create("tags");
  cl_assert_(tags && tags->len > 0, "couldn't read test tags file");
}

void test_tags__cleanup(void) {
  tags_clear(tags);
  tags = NULL;
  cl_fixture_cleanup("tags");
}

void test_tags__find(void) {
  struct tag_t *main = tags_find(tags, "main");
  cl_assert_(main, "expected tag 'main' to exist");
  cl_assert_equal_s("main", main->name);
  cl_assert_equal_s("main.c", main->path);
  cl_assert_equal_s("/^int main\\(int argc, char \\*argv\\[\\]\\) \\{$", main->cmd);

  struct tag_t *garbage = tags_find(tags, "garbage");
  cl_assert_(!garbage, "expected tag 'garbage' not to exist");
}

void test_tags__updated(void) {
  time_t first_loaded = tags->loaded_at;
  sleep(1);
  utimes(tags->file, NULL);
  tags_find(tags, "main");
  cl_assert_(tags->loaded_at > first_loaded, "expected reloaded tags file");
}

void test_tags__deleted(void) {
  cl_assert_(tags->len > 0, "expected there to be tags initially");
  remove(tags->file);
  struct tag_t *main = tags_find(tags, "main");
  cl_assert_(!main, "expected tag not to be found after deleting file");
  cl_assert_equal_i(tags->len, 0);
}
