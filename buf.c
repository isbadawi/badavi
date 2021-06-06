#include "buf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <termbox.h>

#include "util.h"

void buf_grow(struct buf *buf, size_t cap) {
  buf->buf = xrealloc(buf->buf, cap);
  buf->cap = cap;
}

struct buf *buf_create(size_t cap) {
  struct buf *buf = xmalloc(sizeof(*buf));

  buf->buf = NULL;
  buf->cap = 0;
  buf_grow(buf, cap);

  buf_clear(buf);
  buf->cap = cap;
  return buf;
}

struct buf *buf_from_cstr(char *s) {
  struct buf *buf = buf_create(max(1, strlen(s)) * 2);
  buf_append(buf, s);
  return buf;
}

struct buf *buf_from_char(char c) {
  char s[2] = {c, '\0'};
  return buf_from_cstr(s);
}

struct buf *buf_from_utf8(uint32_t ch) {
  char s[8];
  int len = tb_utf8_unicode_to_char(s, ch);
  s[len] = '\0';
  return buf_from_cstr(s);
}

struct buf *buf_copy(struct buf *buf) {
  return buf_from_cstr(buf->buf);
}

void buf_free(struct buf *buf) {
  free(buf->buf);
  free(buf);
}

void buf_clear(struct buf *buf) {
  buf->buf[0] = '\0';
  buf->len = 0;
}

void buf_insert(struct buf *buf, char *s, size_t pos) {
  size_t len = strlen(s);
  size_t new_len = buf->len + len;

  size_t new_cap = buf->cap;
  if (new_len + 1 >= new_cap) {
    while (new_len + 1 >= new_cap) {
      new_cap *= 2;
    }
    buf_grow(buf, new_cap);
  }

  // Move the bit after the insertion...
  memmove(
      buf->buf + pos + len,
      buf->buf + pos,
      buf->len - pos);

  // Stitch the new part in.
  memmove(
      buf->buf + pos,
      s,
      len);

  buf->len += len;
  buf->buf[buf->len] = '\0';
}

void buf_append(struct buf *buf, char *s) {
  buf_insert(buf, s, buf->len);
}

void buf_append_char(struct buf *buf, char c) {
  char s[2] = {c, '\0'};
  buf_append(buf, s);
}

void buf_delete(struct buf *buf, size_t pos, size_t len) {
  if (pos >= buf->len || pos + len > buf->len) {
    return;
  }

  memmove(
      buf->buf + pos,
      buf->buf + pos + len,
      buf->len - (pos + len));

  buf->len -= len;
  buf->buf[buf->len] = '\0';
}

void buf_printf(struct buf *buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(buf, format, args);
  va_end(args);
}

void buf_appendf(struct buf *buf, const char *format, ...) {
  struct buf *suffix = buf_create(1);
  va_list args;
  va_start(args, format);
  buf_vprintf(suffix, format, args);
  va_end(args);
  buf_append(buf, suffix->buf);
  buf_free(suffix);
}

void buf_vprintf(struct buf *buf, const char *format, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  // Try once...
  size_t n = (size_t) vsnprintf(buf->buf, buf->cap, format, args);

  // vsnprintf returns the required size if it wasn't enough, so grow to that
  // size and try again.
  if (n >= buf->cap) {
    buf_grow(buf, n + 1);
    n = (size_t) vsnprintf(buf->buf, buf->cap, format, args_copy);
  }

  va_end(args_copy);
  buf->len = n;
}

bool buf_equals(struct buf *buf, char *s) {
  return !strcmp(buf->buf, s);
}

bool buf_startswith(struct buf *buf, char *prefix) {
  return !strncmp(buf->buf, prefix, strlen(prefix));
}

bool buf_endswith(struct buf *buf, char *suffix) {
  size_t suffix_len = strlen(suffix);
  if (buf->len < suffix_len) {
    return false;
  }
  return !strcmp(buf->buf + (buf->len - suffix_len), suffix);
}

void buf_strip_whitespace(struct buf *buf) {
  while (buf->len && isspace(buf->buf[0])) {
    buf_delete(buf, 0, 1);
  }
  while (buf->len && isspace(buf->buf[buf->len - 1])) {
    buf_delete(buf, buf->len - 1, 1);
  }
}

static void intbuf_grow(struct intbuf *buf, size_t cap) {
  buf->buf = xrealloc(buf->buf, cap * sizeof(*buf->buf));
  buf->cap = cap;
}

struct intbuf *intbuf_create(size_t cap) {
  struct intbuf *buf = xmalloc(sizeof(*buf));

  buf->buf = NULL;
  buf->cap = 0;
  intbuf_grow(buf, cap);

  buf->len = 0;
  buf->cap = cap;
  return buf;
}

void intbuf_free(struct intbuf *buf) {
  free(buf->buf);
  free(buf);
}

void intbuf_insert(struct intbuf *buf, unsigned int i, size_t pos) {
  if (buf->len == buf->cap) {
    intbuf_grow(buf, 2 * buf->cap);
  }

  memmove(buf->buf + pos + 1, buf->buf + pos, (buf->len++ - pos) * sizeof(*buf->buf));
  buf->buf[pos] = i;
}

void intbuf_add(struct intbuf *buf, unsigned int i) {
  intbuf_insert(buf, i, buf->len);
}

void intbuf_remove(struct intbuf *buf, size_t pos) {
  memmove(buf->buf + pos, buf->buf + pos + 1, (buf->len-- - pos) * sizeof(*buf->buf));
}
