#include "options.h"

#include <stdlib.h>
#include <string.h>

#include <assert.h>

typedef struct {
  const char *name;

  enum option_type_t {
    OPTION_TYPE_INT,
    OPTION_TYPE_BOOL,
  } type;

  union {
    int i;
  } value;
} option_t;

#define OPTION(name, type, defaultval) {#name, type, {defaultval}}

#define BOOL_OPTION(name, defaultval) OPTION(name, OPTION_TYPE_BOOL, defaultval)
#define INT_OPTION(name, defaultval) OPTION(name, OPTION_TYPE_INT, defaultval)

static option_t option_table[] = {
  BOOL_OPTION(number, 0),
  BOOL_OPTION(relativenumber, 0),
  INT_OPTION(numberwidth, 4),
  BOOL_OPTION(ignorecase, 0),
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

static int option_has_type(const char *name, enum option_type_t type) {
  option_t *option = option_find(name);
  return option && option->type == type;
}

int option_exists(const char *name) {
  return option_find(name) != NULL;
}

int option_is_bool(const char *name) {
  return option_has_type(name, OPTION_TYPE_BOOL);
}

int option_get_bool(const char *name) {
  assert(option_is_bool(name));
  return option_find(name)->value.i;
}

void option_set_bool(const char *name, int value) {
  assert(option_is_bool(name));
  option_find(name)->value.i = value;
}

int option_is_int(const char *name) {
  return option_has_type(name, OPTION_TYPE_INT);
}

int option_get_int(const char *name) {
  assert(option_is_int(name));
  return option_find(name)->value.i;
}

void option_set_int(const char *name, int value) {
  assert(option_is_int(name));
  option_find(name)->value.i = value;
}
