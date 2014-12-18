#ifndef _buf_h_included
#define _buf_h_included

// buf_t is a simple growable array.

typedef struct {
  char *buf;
  int len;
  int cap;
} buf_t;

buf_t *buf_create(int cap);
buf_t *buf_from_cstr(const char *s);

int buf_delete(buf_t *buf, int pos, int len);
int buf_insert(buf_t *buf, const char *s, int pos);

#endif
