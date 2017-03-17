#pragma once

#include <stddef.h>
#include <time.h>

struct editor;

struct tag {
  char *name;
  char *path;
  char *cmd;
};

struct tag_jump {
  struct buffer *buffer;
  size_t cursor;
  struct tag *tag;
};

struct tags {
  struct tag *tags;
  size_t len;

  const char *file;
  time_t loaded_at;
};

struct tags *tags_create(const char *path);
void tags_clear(struct tags *tags);
struct tag *tags_find(struct tags *tags, char *name);

void editor_jump_to_tag(struct editor *editor, char *tag);

void editor_tag_stack_prev(struct editor *editor);
void editor_tag_stack_next(struct editor *editor);
