#pragma once

#include <stdarg.h>
#include <stddef.h>

// buf_t is a simple growable string.
struct buf_t {
  char *buf;
  size_t len;
  size_t cap;
};

struct buf_t *buf_create(size_t cap);
struct buf_t *buf_from_cstr(char *s);
struct buf_t *buf_from_char(char c);
struct buf_t *buf_copy(struct buf_t *buf);
void buf_free(struct buf_t *buf);

void buf_clear(struct buf_t *buf);
void buf_grow(struct buf_t *buf, size_t cap);
void buf_delete(struct buf_t *buf, size_t pos, size_t len);
void buf_insert(struct buf_t *buf, char *s, size_t pos);
void buf_append(struct buf_t *buf, char *s);

// Write the formatted data to buf (overwriting what was there),
// automatically growing it if needed.
__attribute__((__format__(__printf__, 2, 3)))
void buf_printf(struct buf_t *buf, const char *format, ...);
__attribute__((__format__(__printf__, 2, 0)))
void buf_vprintf(struct buf_t *buf, const char *format, va_list args);

// Similar to struct buf_t but for ints.
struct intbuf_t {
  unsigned int *buf;
  size_t len;
  size_t cap;
};

struct intbuf_t *intbuf_create(size_t cap);
void intbuf_free(struct intbuf_t *buf);

void intbuf_insert(struct intbuf_t *buf, unsigned int i, size_t pos);
void intbuf_add(struct intbuf_t *buf, unsigned int i);
void intbuf_remove(struct intbuf_t *buf, size_t pos);
