#pragma once

#include <stddef.h>
#include <time.h>

struct tag_t {
  char *name;
  char *path;
  char *cmd;
};

struct tags_t {
  struct tag_t *tags;
  size_t len;

  char *file;
  time_t loaded_at;
};

struct tags_t *tags_create(char *path);
struct tag_t *tags_find(struct tags_t *tags, char *name);
