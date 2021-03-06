#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "attrs.h"

// struct buf is a simple growable string.
struct buf {
  char *buf;
  size_t len;
  size_t cap;
};

struct buf *buf_create(size_t cap);
struct buf *buf_from_cstr(char *s);
struct buf *buf_from_char(char c);
struct buf *buf_from_utf8(uint32_t c);
struct buf *buf_copy(struct buf *buf);
void buf_free(struct buf *buf);

void buf_clear(struct buf *buf);
void buf_grow(struct buf *buf, size_t cap);
void buf_delete(struct buf *buf, size_t pos, size_t len);
void buf_insert(struct buf *buf, char *s, size_t pos);
void buf_append(struct buf *buf, char *s);
void buf_append_char(struct buf *buf, char c);

// Write the formatted data to buf (overwriting what was there),
// automatically growing it if needed.
ATTR_PRINTFLIKE(2, 3)
void buf_printf(struct buf *buf, const char *format, ...);
ATTR_PRINTFLIKE(2, 3)
void buf_appendf(struct buf *buf, const char *format, ...);
ATTR_PRINTFLIKE(2, 0)
void buf_vprintf(struct buf *buf, const char *format, va_list args);

bool buf_equals(struct buf *buf, char *s);
bool buf_startswith(struct buf *buf, char *prefix);
bool buf_endswith(struct buf *buf, char *suffix);
void buf_strip_whitespace(struct buf *buf);

// Similar to struct buf but for ints.
struct intbuf {
  unsigned int *buf;
  size_t len;
  size_t cap;
};

struct intbuf *intbuf_create(size_t cap);
void intbuf_free(struct intbuf *buf);

void intbuf_insert(struct intbuf *buf, unsigned int i, size_t pos);
void intbuf_add(struct intbuf *buf, unsigned int i);
void intbuf_remove(struct intbuf *buf, size_t pos);
