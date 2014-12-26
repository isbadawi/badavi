#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gap.h"

static buffer_t *buffer_of(char *path, gapbuf_t *gb) {
  buffer_t *buffer = malloc(sizeof(buffer_t));
  if (!buffer) {
    return NULL;
  }

  buffer->text = gb;
  strcpy(buffer->name, path ? path : "");
  buffer->dirty = 0;
  buffer->next = NULL;

  return buffer;
}

buffer_t *buffer_create(char *path) {
  gapbuf_t *gb = gb_create();
  return gb ? buffer_of(path, gb) : NULL;
}

buffer_t *buffer_open(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  gapbuf_t *gb = gb_load(fp);
  fclose(fp);

  return gb ? buffer_of(path, gb) : NULL;
}

int buffer_write(buffer_t *buffer) {
  if (!buffer->name[0]) {
    return -1;
  }
  return buffer_saveas(buffer, buffer->name);
}

int buffer_saveas(buffer_t *buffer, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return -1;
  }

  if (!buffer->name[0]) {
    strcpy(buffer->name, path);
  }

  gb_save(buffer->text, fp);
  buffer->dirty = 0;
  fclose(fp);
  return 0;
}
