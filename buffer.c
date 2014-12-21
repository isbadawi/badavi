#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "util.h"

buffer_t *buffer_create(void) {
  buffer_t *buffer = malloc(sizeof(buffer_t));
  if (!buffer) {
    return NULL;
  }

  buffer->head = malloc(sizeof(line_t));
  if (!buffer->head) {
    free(buffer);
    return NULL;
  }

  buffer->name = NULL;
  buffer->head->buf = NULL;
  buffer->head->prev = NULL;
  buffer->head->next = NULL;
  buffer->nlines = 0;
  buffer->next = NULL;

  buffer_insert_line(buffer, "", 0);

  return buffer;
}

buffer_t *buffer_open(char *path) {
  buffer_t *buffer = buffer_create();
  if (!buffer) {
    return NULL;
  }
  buffer_remove_line(buffer, buffer->head->next);

  FILE *fp = fopen(path, "r");
  if (!fp) {
    free(buffer->head);
    free(buffer);
    return NULL;
  }
  buffer->name = path;

  char chunk[1024 + 1];
  line_t *last_line = NULL;
  int n;
  while ((n = fread(chunk, 1, 1024, fp))) {
    char *start = chunk;
    for (int i = 0; i < n; ++i) {
      if (chunk[i] == '\n') {
        chunk[i] = '\0';
        if (last_line) {
          buf_insert(last_line->buf, start, last_line->buf->len);
          last_line = NULL;
        } else {
          buffer_insert_line(buffer, start, buffer->nlines);
        }
        start = chunk + i + 1;
      }
    }
    // Add the rest of this chunk to a new line that we'll append to later.
    chunk[n] = '\0';
    if (strlen(start) != 0) {
      last_line = buffer_insert_line(buffer, start, buffer->nlines);
    }
  }

  return buffer;
}

int buffer_size(buffer_t *buffer) {
  int size = 0;
  for (line_t *line = buffer->head->next; line != NULL; line = line->next) {
    size += line->buf->len + 1;
  }
  return size;
}


int buffer_write(buffer_t *buffer) {
  if (!buffer->name) {
    return -1;
  }
  return buffer_saveas(buffer, buffer->name);
}

int buffer_saveas(buffer_t *buffer, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return -1;
  }

  if (!buffer->name) {
    buffer->name = path;
  }

  for (line_t *line = buffer->head->next; line != NULL; line = line->next) {
    fwrite(line->buf->buf, 1, line->buf->len, fp);
    fputc('\n', fp);
  }

  fclose(fp);
  return 0;
}

static line_t *line_create(char *s) {
  line_t *line = malloc(sizeof(line_t));
  if (!line) {
    return NULL;
  }

  line->buf = buf_from_cstr(s);
  if (!line->buf) {
    free(line);
    return NULL;
  }

  line->prev = NULL;
  line->next = NULL;
  return line;
}

line_t *buffer_insert_line(buffer_t *buffer, char *s, int pos) {
  if (pos < 0 || pos > buffer->nlines) {
    return NULL;
  }

  line_t *prev = buffer->head;
  for (int i = 0; i < pos; ++i) {
    prev = prev->next;
  }

  return buffer_insert_line_after(buffer, s, prev);
}

line_t *buffer_insert_line_after(buffer_t *buffer, char *s, line_t *line) {
  line_t *new = line_create(s);
  if (!new) {
    return NULL;
  }

  new->next = line->next;
  if (new->next) {
    new->next->prev = new;
  }
  line->next = new;
  new->prev = line;

  buffer->nlines++;

  return new;
}

void buffer_remove_line(buffer_t *buffer, line_t *line) {
  line->prev->next = line->next;
  if (line->next) {
    line->next->prev = line->prev;
  }
  buf_free(line->buf);
  free(line);

  buffer->nlines--;
}

int buffer_index_of_line(buffer_t *buffer, line_t *line) {
  line_t *l = buffer->head->next;
  for (int i = 0; l != NULL; i++, l = l->next) {
    if (l == line) {
      return i;
    }
  }
  return -1;
}
