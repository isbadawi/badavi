#pragma once

#include <stddef.h>
#include <time.h>

typedef struct {
  char *name;
  char *path;
  char *cmd;
} tag_t;

typedef struct {
  tag_t *tags;
  size_t len;

  char *file;
  time_t loaded_at;
} tags_t;

tags_t *tags_create(char *path);
tag_t *tags_find(tags_t *tags, char *name);
