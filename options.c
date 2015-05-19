#include "options.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  enum { OPTION_TYPE_INT } type;
  union {
    int i;
  } value;
} option_t;

#define INT_OPTION(name, defaultval) \
 {#name, OPTION_TYPE_INT, {defaultval}}

static option_t option_table[] = {
  INT_OPTION(number, 1),
  {NULL, -1}
};

static option_t *option_find(const char *name) {
  for (int i = 0; option_table[i].name; ++i) {
    if (!strcmp(option_table[i].name, name)) {
      return &option_table[i];
    }
  }
  return NULL;
}

int option_exists(const char *name) {
  return option_find(name) != NULL;
}

int option_get_int(const char *name) {
  return option_find(name)->value.i;
}

void option_set_int(const char *name, int value) {
  option_find(name)->value.i = value;
}
