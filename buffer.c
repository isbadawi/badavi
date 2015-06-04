#include "buffer.h"

#include <stdio.h>
#include <string.h>

#include "gap.h"
#include "list.h"
#include "util.h"

static struct buffer_t *buffer_of(char *path, struct gapbuf_t *gb) {
  struct buffer_t *buffer = xmalloc(sizeof(*buffer));

  buffer->text = gb;
  strcpy(buffer->name, path ? path : "");
  buffer->dirty = false;

  buffer->undo_stack = list_create();
  buffer->redo_stack = list_create();

  return buffer;
}

struct buffer_t *buffer_create(char *path) {
  return buffer_of(path, gb_create());
}

struct buffer_t *buffer_open(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  struct gapbuf_t *gb = gb_load(fp);
  fclose(fp);

  return buffer_of(path, gb);
}

int buffer_write(struct buffer_t *buffer) {
  if (!buffer->name[0]) {
    return -1;
  }
  return buffer_saveas(buffer, buffer->name);
}

int buffer_saveas(struct buffer_t *buffer, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return -1;
  }

  if (!buffer->name[0]) {
    strcpy(buffer->name, path);
  }

  gb_save(buffer->text, fp);
  buffer->dirty = false;
  fclose(fp);
  return 0;
}
