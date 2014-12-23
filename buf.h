#ifndef _buf_h_included
#define _buf_h_included

#include <stdarg.h>

// buf_t is a simple growable array.

typedef struct {
  char *buf;
  int len;
  int cap;
} buf_t;

buf_t *buf_create(int cap);
buf_t *buf_from_cstr(const char *s);
void buf_free(buf_t *buf);

int buf_delete(buf_t *buf, int pos, int len);
int buf_insert(buf_t *buf, const char *s, int pos);
int buf_append(buf_t *buf, const char *s);

// Write the formatted data to buf (overwriting what was there),
// automatically growing it if needed.
void buf_printf(buf_t *buf, const char *format, ...);
void buf_vprintf(buf_t *buf, const char *format, va_list args);

#endif
