#include "gap.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

#define GAPSIZE 1024

gapbuf_t *gb_create(void) {
  gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  gb->bufstart = malloc(1 + GAPSIZE);
  if (!gb->bufstart) {
    free(gb);
    return NULL;
  }

  gb->gapstart = gb->bufstart;
  gb->gapend = gb->gapstart + GAPSIZE;
  gb->bufend = gb->bufstart + GAPSIZE + 1;
  gb->bufend[-1] = '\n';

  gb->lines = intbuf_create(10);
  intbuf_add(gb->lines, 0);
  return gb;
}

gapbuf_t *gb_load(FILE *fp) {
  gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  struct stat info;
  fstat(fileno(fp), &info);
  int filesize = info.st_size;
  int bufsize = filesize + GAPSIZE;

  gb->bufstart = malloc(bufsize);
  if (!gb->bufstart) {
    free(gb);
    return NULL;
  }

  gb->gapstart = gb->bufstart;
  gb->gapend = gb->bufstart + GAPSIZE;
  gb->bufend = gb->bufstart + bufsize;

  fread(gb->gapend, 1, filesize, fp);

  gb->lines = intbuf_create(10);
  int last = -1;
  for (int i = 0; i < filesize; ++i) {
    if (gb->gapend[i] == '\n') {
      intbuf_add(gb->lines, i - last - 1);
      last = i;
    }
  }

  return gb;
}

void gb_free(gapbuf_t *gb) {
  free(gb->bufstart);
  intbuf_free(gb->lines);
}

int gb_size(gapbuf_t *gb) {
  return (gb->gapstart - gb->bufstart) + (gb->bufend - gb->gapend);
}

int gb_nlines(gapbuf_t *gb) {
  return gb->lines->len;
}

void gb_save(gapbuf_t *gb, FILE *fp) {
  fwrite(gb->bufstart, 1, gb->gapstart - gb->bufstart, fp);
  fwrite(gb->gapend, 1, gb->bufend - gb->gapend, fp);
}

// Returns the real index of the logical offset pos.
static int gb_index(gapbuf_t *gb, int pos) {
  int gapsize = gb->gapend - gb->gapstart;
  return gb->bufstart + pos < gb->gapstart ? pos : pos + gapsize;
}

char gb_getchar(gapbuf_t *gb, int pos) {
  return gb->bufstart[gb_index(gb, pos)];
}

// Moves the gap so that gb->bufstart + pos == gb->gapstart.
static void gb_mvgap(gapbuf_t *gb, int pos) {
  char *point = gb->bufstart + gb_index(gb, pos);
  if (gb->gapend <= point) {
    int n = point - gb->gapend;
    memcpy(gb->gapstart, gb->gapend, n);
    gb->gapstart += n;
    gb->gapend += n;
  } else if (point < gb->gapstart) {
    int n = gb->gapstart - point;
    memcpy(gb->gapend - n, point, n);
    gb->gapstart -= n;
    gb->gapend -= n;
  }
}

// Ensure the gap fits at least n new characters.
static void gb_growgap(gapbuf_t *gb, int n) {
  int gapsize = gb->gapend - gb->gapstart;
  if (n <= gapsize) {
    return;
  }

  int newgapsize = 0;
  while (newgapsize < 2*n) {
    newgapsize += GAPSIZE;
  }
  // Pointers will be obsoleted so remember offsets...
  int leftsize = gb->gapstart - gb->bufstart;
  int rightsize = gb->bufend - gb->gapend;

  int newsize = leftsize + rightsize + newgapsize;

  gb->bufstart = realloc(gb->bufstart, newsize);
  gb->gapstart = gb->bufstart + leftsize;
  gb->gapend = gb->gapstart + newgapsize;
  // Move the bit after the gap right by gapsize positions...
  memcpy(gb->gapend, gb->gapstart + gapsize, rightsize);
  gb->bufend = gb->bufstart + newsize;
}

void gb_putchar(gapbuf_t *gb, char c, int pos) {
  gb_putstring(gb, &c, 1, pos);
}

void gb_putstring(gapbuf_t *gb, char *buf, int n, int pos) {
  gb_growgap(gb, n);
  gb_mvgap(gb, pos);
  memcpy(gb->gapstart, buf, n);

  int line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  // Adjust the line lengths.

  // Where we started inserting on this line
  int start = col;
  // Characters since last newline
  int last = 0;
  for (int i = 0; i < n; ++i) {
    if (gb->gapstart[i] == '\n') {
      int oldlen = gb->lines->buf[line];
      gb->lines->buf[line] = start + last;
      intbuf_insert(gb->lines, oldlen - (start + last), ++line);
      start = last = 0;
    } else {
      gb->lines->buf[line]++;
      last++;
    }
  }
  gb->gapstart += n;
}

void gb_del(gapbuf_t *gb, int n, int pos) {
  gb_mvgap(gb, pos);

  int line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  for (int i = 0; i < n; ++i) {
    if (gb->gapstart[-i - 1] == '\n') {
      gb->lines->buf[line - 1] += gb->lines->buf[line];
      intbuf_remove(gb->lines, line);
      line--;
    } else {
      gb->lines->buf[line]--;
    }
  }

  gb->gapstart -= n;
}

int gb_indexof(gapbuf_t *gb, char c, int start) {
  int size = gb_size(gb);
  for (int i = start; i < size; ++i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return size;
}

int gb_lastindexof(gapbuf_t *gb, char c, int start) {
  for (int i = start; i >= 0; --i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return -1;
}

void gb_pos_to_linecol(gapbuf_t *gb, int pos, int *line, int *column) {
  int offset = 0;
  for (int i = 0; i < gb->lines->len; ++i) {
    int len = gb->lines->buf[i];
    if (offset <= pos && pos <= offset + len) {
      *line = i;
      *column = pos - offset;
      return;
    }
    offset += len + 1;
  }
}

int gb_linecol_to_pos(gapbuf_t *gb, int line, int column) {
  int offset = 0;
  for (int i = 0; i < line; ++i) {
    offset += gb->lines->buf[i] + 1;
  }
  return offset + column;
}
