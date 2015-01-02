#include "buf.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "util.h"

int buf_grow(buf_t *buf, int cap) {
  if (buf->cap >= cap) {
    return 0;
  }
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

  buf->cap = 0;
  if (buf_grow(buf, cap) < 0) {
    free(buf);
    return NULL;
  }

  buf->len = 0;
  buf->cap = cap;
  return buf;
}

buf_t *buf_from_cstr(char *s) {
  buf_t *buf = buf_create(max(1, strlen(s)) * 2);
  if (!buf) {
    return NULL;
  }

  buf_append(buf, s);
  return buf;
}

void buf_free(buf_t *buf) {
  free(buf->buf);
}

void buf_clear(buf_t *buf) {
  buf->len = 0;
}

int buf_insert(buf_t *buf, char *s, int pos) {
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
  return 0;
}

int buf_append(buf_t *buf, char *s) {
  return buf_insert(buf, s, buf->len);
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


static int intbuf_grow(intbuf_t *buf, int cap) {
  buf->buf = realloc(buf->buf, cap * sizeof(int));
  if (!buf->buf) {
    return -1;
  }
  buf->cap = cap;
  return 0;
}

intbuf_t *intbuf_create(int cap) {
  intbuf_t *buf = malloc(sizeof(intbuf_t));
  if (!buf) {
    return NULL;
  }
  buf->buf = NULL;

  if (intbuf_grow(buf, cap) < 0) {
    free(buf);
    return NULL;
  }

  buf->len = 0;
  buf->cap = cap;
  return buf;
}

void intbuf_free(intbuf_t *buf) {
  free(buf->buf);
}

void intbuf_insert(intbuf_t *buf, int i, int pos) {
  if (buf->len == buf->cap) {
    intbuf_grow(buf, 2 * buf->cap);
  }

  memmove(buf->buf + pos + 1, buf->buf + pos, (buf->len++ - pos) * sizeof(int));
  buf->buf[pos] = i;
}

void intbuf_add(intbuf_t *buf, int i) {
  intbuf_insert(buf, i, buf->len);
}

void intbuf_remove(intbuf_t *buf, int i) {
  memmove(buf->buf + i, buf->buf + i + 1, (buf->len-- - i) * sizeof(int));
}
