#include "buf.h"

#include <stdlib.h>
#include <string.h>

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

  buf_append(buf, s);
  return buf;
}

int buf_append(buf_t *buf, const char *s) {
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

  strcpy(buf->buf + buf->len, s);
  buf->len += len;
  return 0;
}

int buf_delete(buf_t *buf, int pos, int len) {
  if (pos < 0 || pos >= buf->len || pos + len >= buf->len) {
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
