#include "gap.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <termbox.h>

#include "buf.h"
#include "util.h"

#define GAPSIZE 1024

struct gapbuf *gb_create(void) {
  struct gapbuf *gb = xmalloc(sizeof(*gb));

  gb->bufstart = xmalloc(1 + GAPSIZE);
  gb->gapstart = gb->bufstart;
  gb->gapend = gb->gapstart + GAPSIZE;
  gb->bufend = gb->bufstart + GAPSIZE + 1;
  gb->bufend[-1] = '\n';

  gb->lines = intbuf_create(10);
  intbuf_add(gb->lines, 0);
  return gb;
}

static struct gapbuf *gb_load(FILE *fp, size_t filesize) {
  struct gapbuf *gb = xmalloc(sizeof(*gb));
  size_t bufsize = filesize + GAPSIZE;

  gb->bufstart = xmalloc(bufsize + 1);
  gb->gapstart = gb->bufstart;
  gb->gapend = gb->bufstart + GAPSIZE;
  gb->bufend = gb->bufstart + bufsize;

  fread(gb->gapend, 1, filesize, fp);
  fclose(fp);

  if (gb->bufend[-1] != '\n') {
    gb->bufend[0] = '\n';
    gb->bufend++;
    filesize++;
  }

  gb->lines = intbuf_create(10);
  ssize_t last = -1;
  for (ssize_t i = 0; (size_t) i < filesize; ++i) {
    if (gb->gapend[i] == '\n') {
      unsigned int len = (unsigned int) (i - last) - 1;
      intbuf_add(gb->lines, len);
      last = (ssize_t) i;
    }
  }

  return gb;
}

struct gapbuf *gb_fromfile(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  struct stat info;
  fstat(fileno(fp), &info);
  size_t filesize = (size_t) info.st_size;
  return gb_load(fp, filesize);
}

struct gapbuf *gb_fromstring(struct buf *buf) {
  FILE *fp = fmemopen(buf->buf, buf->len, "r");
  size_t len = buf->len;
  return gb_load(fp, len);
}

void gb_free(struct gapbuf *gb) {
  free(gb->bufstart);
  intbuf_free(gb->lines);
  free(gb);
}

size_t gb_size(struct gapbuf *gb) {
  return (size_t) ((gb->gapstart - gb->bufstart) + (gb->bufend - gb->gapend));
}

size_t gb_nlines(struct gapbuf *gb) {
  return gb->lines->len;
}

void gb_save(struct gapbuf *gb, FILE *fp) {
  fwrite(gb->bufstart, 1, (size_t) (gb->gapstart - gb->bufstart), fp);
  fwrite(gb->gapend, 1, (size_t) (gb->bufend - gb->gapend), fp);
}

// Returns the real index of the logical offset pos.
static size_t gb_index(struct gapbuf *gb, size_t pos) {
  ptrdiff_t gapsize = gb->gapend - gb->gapstart;
  return gb->bufstart + pos < gb->gapstart ? pos : pos + (size_t) gapsize;
}

char gb_getchar(struct gapbuf *gb, size_t pos) {
  return gb->bufstart[gb_index(gb, pos)];
}

int gb_utf8len(struct gapbuf *gb, size_t pos) {
  return tb_utf8_char_length(gb_getchar(gb, pos));
}

uint32_t gb_utf8(struct gapbuf *gb, size_t pos) {
  char buf[8];
  int clen = gb_utf8len(gb, pos);
  for (int i = 0; i < clen; ++i) {
    buf[i] = gb_getchar(gb, pos + i);
  }
  uint32_t ch;
  int ulen = tb_utf8_char_to_unicode(&ch, buf);
  assert(clen == ulen);
  return ch;
}

struct buf *gb_getstring(struct gapbuf *gb, size_t pos, size_t n) {
  struct buf *strbuf = buf_create(n + 1);
  char *buf = strbuf->buf;

  char *start = gb->bufstart + gb_index(gb, pos);
  char *end = gb->bufstart + gb_index(gb, pos + n);
  if (end < gb->gapstart || start >= gb->gapend) {
    memcpy(buf, start, n);
  } else {
    assert(gb->gapstart >= start);
    assert(end >= gb->gapend);
    size_t l = (size_t)(gb->gapstart - start);
    size_t r = (size_t)(end - gb->gapend);
    memcpy(buf, start, l);
    memcpy(buf + l, gb->gapend, r);
  }
  buf[n] = '\0';
  strbuf->len = n;
  return strbuf;
}

struct buf *gb_getline(struct gapbuf *gb, size_t pos) {
  size_t line, column;
  gb_pos_to_linecol(gb, pos, &line, &column);
  return gb_getstring(gb, pos - column, gb->lines->buf[line]);
}

// Moves the gap so that gb->bufstart + pos == gb->gapstart.
void gb_mvgap(struct gapbuf *gb, size_t pos) {
  char *point = gb->bufstart + gb_index(gb, pos);
  if (gb->gapend <= point) {
    size_t n = (size_t)(point - gb->gapend);
    memcpy(gb->gapstart, gb->gapend, n);
    gb->gapstart += n;
    gb->gapend += n;
  } else if (point < gb->gapstart) {
    size_t n = (size_t)(gb->gapstart - point);
    memcpy(gb->gapend - n, point, n);
    gb->gapstart -= n;
    gb->gapend -= n;
  }
}

// Ensure the gap fits at least n new characters.
static void gb_growgap(struct gapbuf *gb, size_t n) {
  ptrdiff_t gapsize = gb->gapend - gb->gapstart;
  if (n <= (size_t) gapsize) {
    return;
  }

  size_t newgapsize = 0;
  while (newgapsize < 2*n) {
    newgapsize += GAPSIZE;
  }
  // Pointers will be obsoleted so remember offsets...
  ptrdiff_t leftsize = gb->gapstart - gb->bufstart;
  ptrdiff_t rightsize = gb->bufend - gb->gapend;

  size_t newsize = (size_t) (leftsize + rightsize) + newgapsize;

  gb->bufstart = xrealloc(gb->bufstart, newsize);
  gb->gapstart = gb->bufstart + leftsize;
  gb->gapend = gb->gapstart + newgapsize;
  assert(rightsize >= 0);
  // Move the bit after the gap right by gapsize positions...
  memcpy(gb->gapend, gb->gapstart + gapsize, (size_t)rightsize);
  gb->bufend = gb->bufstart + newsize;
}

void gb_putchar(struct gapbuf *gb, char c, size_t pos) {
  gb_putstring(gb, &c, 1, pos);
}

void gb_putstring(struct gapbuf *gb, char *buf, size_t n, size_t pos) {
  gb_growgap(gb, n);
  gb_mvgap(gb, pos);
  memcpy(gb->gapstart, buf, n);

  size_t line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  // Adjust the line lengths.

  // Where we started inserting on this line
  size_t start = col;
  // Characters since last newline
  size_t last = 0;
  for (size_t i = 0; i < n; ++i) {
    if (gb->gapstart[i] == '\n') {
      size_t oldlen = gb->lines->buf[line];
      size_t newlen = start + last;
      gb->lines->buf[line] = (unsigned int) newlen;
      intbuf_insert(gb->lines, (unsigned int) (oldlen - newlen), ++line);
      start = last = 0;
    } else {
      gb->lines->buf[line]++;
      last++;
    }
  }
  gb->gapstart += n;
}

void gb_del(struct gapbuf *gb, size_t n, size_t pos) {
  gb_mvgap(gb, pos);

  size_t line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  for (size_t i = 0; i < n; ++i) {
    if (gb->gapstart[-i - 1] == '\n') {
      gb->lines->buf[line - 1] += gb->lines->buf[line];
      intbuf_remove(gb->lines, line);
      line--;
    } else {
      gb->lines->buf[line]--;
    }
  }

  gb->gapstart -= n;

  // Empty files are tricky for us, so insert a newline if needed...
  if (!gb_size(gb)) {
    *(gb->gapstart++) = '\n';
    intbuf_add(gb->lines, 0);
  }
}

size_t gb_indexof(struct gapbuf *gb, char c, size_t start) {
  size_t size = gb_size(gb);
  for (size_t i = start; i < size; ++i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return size;
}

ssize_t gb_lastindexof(struct gapbuf *gb, char c, size_t start) {
  for (ssize_t i = (ssize_t) start; i >= 0; --i) {
    if (gb_getchar(gb, (size_t) i) == c) {
      return i;
    }
  }
  return -1;
}

void gb_pos_to_linecol(struct gapbuf *gb, size_t pos, size_t *line, size_t *column) {
  size_t offset = 0;
  *line = *column = 0;
  for (size_t i = 0; i < gb->lines->len; ++i) {
    size_t len = gb->lines->buf[i];
    *line = i;
    if (offset <= pos && pos <= offset + len) {
      break;
    }
    offset += len + 1;
  }
  for (size_t i = offset; i != pos; i = gb_utf8next(gb, i)) {
    (*column)++;
  }
}

size_t gb_linecol_to_pos(struct gapbuf *gb, size_t line, size_t column) {
  size_t offset = 0;
  for (size_t i = 0; i < line; ++i) {
    offset += gb->lines->buf[i] + 1;
  }
  for (size_t i = 0; i < column; ++i) {
    offset += gb_utf8len(gb, offset);
  }
  return offset;
}

size_t gb_utf8len_line(struct gapbuf *gb, size_t line) {
  size_t pos = gb_linecol_to_pos(gb, line, 0);
  size_t len = 0;
  while (gb_getchar(gb, pos) != '\n') {
    pos += gb_utf8len(gb, pos);
    len++;
  }
  return len;
}

size_t gb_utf8next(struct gapbuf *gb, size_t pos) {
  return pos + gb_utf8len(gb, pos);
}

static bool isutf8start(char c) {
  return (c & 0xc0) != 0x80;
}

size_t gb_utf8prev(struct gapbuf *gb, size_t pos) {
  do {
    pos--;
  } while (!isutf8start(gb_getchar(gb, pos)));
  return pos;
}
