#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "util.h"

file_t *file_read(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  file_t *file = malloc(sizeof(file_t));
  if (!file) {
    fclose(fp);
    return NULL;
  }

  file->head = malloc(sizeof(line_t));
  if (!file->head) {
    fclose(fp);
    free(file);

    return NULL;
  }
  file->head->buf = NULL;
  file->head->prev = NULL;
  file->head->next = NULL;

  file->nlines = 0;

  char chunk[1024 + 1];
  line_t *last_line = NULL;
  int n;
  while ((n = fread(chunk, 1, 1024, fp))) {
    char *start = chunk;
    for (int i = 0; i < n; ++i) {
      if (chunk[i] == '\n') {
        chunk[i] = '\0';
        debug("line: \"%s\"\n", start);
        if (last_line) {
          buf_insert(last_line->buf, start, last_line->buf->len);
          last_line = NULL;
        } else {
          file_insert_line(file, start, file->nlines);
        }
        start = chunk + i + 1;
      }
    }
    // Add the rest of this chunk to a new line that we'll append to later.
    chunk[n] = '\0';
    if (strlen(start) != 0) {
      last_line = file_insert_line(file, start, file->nlines);
    }
  }

  return file;
}

int file_write(file_t *file, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return -1;
  }

  for (line_t *line = file->head->next; line != NULL; line = line->next) {
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

line_t *file_insert_line(file_t *file, char *s, int pos) {
  if (pos < 0 || pos > file->nlines) {
    return NULL;
  }

  line_t *prev = file->head;
  for (int i = 0; i < pos; ++i) {
    prev = prev->next;
  }

  return file_insert_line_after(file, s, prev);
}

line_t *file_insert_line_after(file_t *file, char *s, line_t *line) {
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

  file->nlines++;

  return new;
}

void file_remove_line(file_t *file, line_t *line) {
  line->prev->next = line->next;
  if (line->next) {
    line->next->prev = line->prev;
  }
  buf_free(line->buf);
  free(line);

  file->nlines--;
}
