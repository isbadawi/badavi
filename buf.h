#ifndef _buf_h_included
#define _buf_h_included

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

// buf_t is a simple growable string.
typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} buf_t;

buf_t *buf_create(size_t cap);
buf_t *buf_from_cstr(char *s);
buf_t *buf_from_char(char c);
buf_t *buf_copy(buf_t *buf);
void buf_free(buf_t *buf);

void buf_clear(buf_t *buf);
bool buf_grow(buf_t *buf, size_t cap);
bool buf_delete(buf_t *buf, size_t pos, size_t len);
bool buf_insert(buf_t *buf, char *s, size_t pos);
bool buf_append(buf_t *buf, char *s);

// Write the formatted data to buf (overwriting what was there),
// automatically growing it if needed.
__attribute__((__format__(__printf__, 2, 3)))
void buf_printf(buf_t *buf, const char *format, ...);
__attribute__((__format__(__printf__, 2, 0)))
void buf_vprintf(buf_t *buf, const char *format, va_list args);

// Similar to buf_t but for ints.
typedef struct {
  unsigned int *buf;
  size_t len;
  size_t cap;
} intbuf_t;

intbuf_t *intbuf_create(size_t cap);
void intbuf_free(intbuf_t *buf);

void intbuf_insert(intbuf_t *buf, unsigned int i, size_t pos);
void intbuf_add(intbuf_t *buf, unsigned int i);
void intbuf_remove(intbuf_t *buf, size_t pos);

#endif
