#include "clar.h"
#include "tags.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static struct tags_t *tags = NULL;

void test_tags__initialize(void) {
  cl_fixture_sandbox("tags");
  tags = tags_create("tags");
  cl_assert(tags && tags->len > 0);
}

void test_tags__cleanup(void) {
  tags_clear(tags);
  tags = NULL;
  cl_fixture_cleanup("tags");
}

void test_tags__find(void) {
  struct tag_t *main = tags_find(tags, "main");
  cl_assert(main);
  cl_assert_equal_s("main", main->name);
  cl_assert_equal_s("main.c", main->path);
  cl_assert_equal_s("/^int main\\(int argc, char \\*argv\\[\\]\\) \\{$", main->cmd);

  struct tag_t *garbage = tags_find(tags, "garbage");
  cl_assert(!garbage);
}

void test_tags__updated(void) {
  time_t first_loaded = --tags->loaded_at;
  utimes(tags->file, NULL);
  tags_find(tags, "main");
  cl_assert(tags->loaded_at > first_loaded);
}

void test_tags__deleted(void) {
  cl_assert(tags->len > 0);
  remove(tags->file);
  struct tag_t *main = tags_find(tags, "main");
  cl_assert(!main);
  cl_assert_equal_i(tags->len, 0);
}
