#pragma once

#include <stddef.h>
#include <time.h>

struct editor_t;

struct tag_t {
  char *name;
  char *path;
  char *cmd;
};

struct tag_jump_t {
  struct buffer_t *buffer;
  size_t cursor;
  struct tag_t *tag;
};

struct tags_t {
  struct tag_t *tags;
  size_t len;

  char *file;
  time_t loaded_at;
};

struct tags_t *tags_create(char *path);
void tags_clear(struct tags_t *tags);
struct tag_t *tags_find(struct tags_t *tags, char *name);

void editor_jump_to_tag(struct editor_t *editor, char *tag);

void editor_tag_stack_prev(struct editor_t *editor);
void editor_tag_stack_next(struct editor_t *editor);
