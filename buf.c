#include "buf.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "util.h"

bool buf_grow(struct buf_t *buf, size_t cap) {
  if (buf->cap >= cap) {
    return true;
  }
  buf->buf = realloc(buf->buf, cap);
  if (!buf->buf) {
    return false;
  }
  buf->cap = cap;
  return true;
}

struct buf_t *buf_create(size_t cap) {
  struct buf_t *buf = malloc(sizeof(struct buf_t));
  if (!buf) {
    return NULL;
  }
  buf->buf = NULL;

  buf->cap = 0;
  if (!buf_grow(buf, cap)) {
    free(buf);
    return NULL;
  }

  buf_clear(buf);
  buf->cap = cap;
  return buf;
}

struct buf_t *buf_from_cstr(char *s) {
  struct buf_t *buf = buf_create(max(1, strlen(s)) * 2);
  if (!buf) {
    return NULL;
  }

  buf_append(buf, s);
  return buf;
}

struct buf_t *buf_from_char(char c) {
  char s[2] = {c, '\0'};
  return buf_from_cstr(s);
}

struct buf_t *buf_copy(struct buf_t *buf) {
  return buf_from_cstr(buf->buf);
}

void buf_free(struct buf_t *buf) {
  free(buf->buf);
}

void buf_clear(struct buf_t *buf) {
  buf->buf[0] = '\0';
  buf->len = 0;
}

bool buf_insert(struct buf_t *buf, char *s, size_t pos) {
  size_t len = strlen(s);
  size_t new_len = buf->len + len;

  size_t new_cap = buf->cap;
  if (new_len + 1 >= new_cap) {
    while (new_len + 1 >= new_cap) {
      new_cap *= 2;
    }
    if (!buf_grow(buf, new_cap)) {
      return false;
    }
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
  return true;
}

bool buf_append(struct buf_t *buf, char *s) {
  return buf_insert(buf, s, buf->len);
}

bool buf_delete(struct buf_t *buf, size_t pos, size_t len) {
  if (pos >= buf->len || pos + len > buf->len) {
    return false;
  }

  memmove(
      buf->buf + pos,
      buf->buf + pos + len,
      buf->len - (pos + len));

  buf->len -= len;
  buf->buf[buf->len] = '\0';
  return true;
}

void buf_printf(struct buf_t *buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(buf, format, args);
  va_end(args);
}

void buf_vprintf(struct buf_t *buf, const char *format, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  // Try once...
  size_t n = (size_t) vsnprintf(buf->buf, buf->cap, format, args);
  va_end(args);

  // vsnprintf returns the required size if it wasn't enough, so grow to that
  // size and try again.
  if (n >= buf->cap) {
    buf_grow(buf, n + 1);
    n = (size_t) vsnprintf(buf->buf, buf->cap, format, args_copy);
    va_end(args_copy);
  }

  buf->len = n;
}


static bool intbuf_grow(struct intbuf_t *buf, size_t cap) {
  buf->buf = realloc(buf->buf, cap * sizeof(unsigned int));
  if (!buf->buf) {
    return false;
  }
  buf->cap = cap;
  return true;
}

struct intbuf_t *intbuf_create(size_t cap) {
  struct intbuf_t *buf = malloc(sizeof(struct intbuf_t));
  if (!buf) {
    return NULL;
  }
  buf->buf = NULL;

  if (!intbuf_grow(buf, cap)) {
    free(buf);
    return NULL;
  }

  buf->len = 0;
  buf->cap = cap;
  return buf;
}

void intbuf_free(struct intbuf_t *buf) {
  free(buf->buf);
}

void intbuf_insert(struct intbuf_t *buf, unsigned int i, size_t pos) {
  if (buf->len == buf->cap) {
    intbuf_grow(buf, 2 * buf->cap);
  }

  memmove(buf->buf + pos + 1, buf->buf + pos, (buf->len++ - pos) * sizeof(unsigned int));
  buf->buf[pos] = i;
}

void intbuf_add(struct intbuf_t *buf, unsigned int i) {
  intbuf_insert(buf, i, buf->len);
}

void intbuf_remove(struct intbuf_t *buf, size_t pos) {
  memmove(buf->buf + pos, buf->buf + pos + 1, (buf->len-- - pos) * sizeof(unsigned int));
}
