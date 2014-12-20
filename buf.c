#include "buf.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "util.h"

static int buf_grow(buf_t *buf, int cap) {
  buf->buf = realloc(buf->buf, cap);
  if (!buf->buf) {
    return -1;
  }
  buf->cap = cap;
  return 0;
}

buf_t *buf_create(int cap) {
  buf_t *buf = malloc(sizeof(buf_t));
  if (!buf) {
    return NULL;
  }
  buf->buf = NULL;

  if (buf_grow(buf, cap) < 0) {
    free(buf);
    return NULL;
  }

  buf->len = 0;
  buf->cap = cap;
  return buf;
}

buf_t *buf_from_cstr(const char *s) {
  buf_t *buf = buf_create(max(1, strlen(s)) * 2);
  if (!buf) {
    return NULL;
  }

  buf_insert(buf, s, buf->len);
  return buf;
}

void buf_free(buf_t *buf) {
  free(buf->buf);
}

int buf_insert(buf_t *buf, const char *s, int pos) {
  int len = strlen(s);
  int new_len = buf->len + len;

  if (new_len + 1 >= buf->cap) {
    while (new_len + 1 >= buf->cap) {
      buf->cap *= 2;
    }
    if (buf_grow(buf, buf->cap) < 0) {
      return -1;
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
  buf->buf[buf->len] = '\0';
  return 0;
}

int buf_delete(buf_t *buf, int pos, int len) {
  if (pos < 0 || pos >= buf->len || pos + len > buf->len) {
    return -1;
  }

  memmove(
      buf->buf + pos,
      buf->buf + pos + len,
      buf->len - (pos + len));

  buf->len -= len;
  buf->buf[buf->len] = '\0';
  return 0;
}

void buf_printf(buf_t *buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf_vprintf(buf, format, args);
  va_end(args);
}

void buf_vprintf(buf_t *buf, const char *format, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  // Try once...
  int n = vsnprintf(buf->buf, buf->cap, format, args);
  va_end(args);

  // vsnprintf returns the required size if it wasn't enough, so grow to that
  // size and try again.
  if (n >= buf->cap) {
    buf_grow(buf, n + 1);
    n = vsnprintf(buf->buf, buf->cap, format, args_copy);
    va_end(args_copy);
  }

  buf->len = n;
}
