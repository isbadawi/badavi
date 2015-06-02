#include "options.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct option_t {
  const char *name;

  enum option_type_t {
    OPTION_TYPE_INT,
    OPTION_TYPE_BOOL,
  } type;

  union {
    bool b;
    int i;
  } value;
};

#define OPTION(name, type, defaultval) {#name, type, {defaultval}}

#define BOOL_OPTION(name, defaultval) OPTION(name, OPTION_TYPE_BOOL, defaultval)
#define INT_OPTION(name, defaultval) OPTION(name, OPTION_TYPE_INT, defaultval)

static struct option_t option_table[] = {
  INT_OPTION(numberwidth, 4),
  BOOL_OPTION(number, false),
  BOOL_OPTION(relativenumber, false),
  BOOL_OPTION(ignorecase, false),
  BOOL_OPTION(smartcase, false),
  BOOL_OPTION(cursorline, false),
  BOOL_OPTION(splitright, false),
  {NULL, OPTION_TYPE_INT, {-1}},
};

static struct option_t *option_find(const char *name) {
  for (int i = 0; option_table[i].name; ++i) {
    if (!strcmp(option_table[i].name, name)) {
      return &option_table[i];
    }
  }
  return NULL;
}

static bool option_has_type(const char *name, enum option_type_t type) {
  struct option_t *option = option_find(name);
  return option && option->type == type;
}

bool option_exists(const char *name) {
  return option_find(name) != NULL;
}

bool option_is_bool(const char *name) {
  return option_has_type(name, OPTION_TYPE_BOOL);
}

bool option_get_bool(const char *name) {
  assert(option_is_bool(name));
  return option_find(name)->value.b;
}

void option_set_bool(const char *name, bool value) {
  assert(option_is_bool(name));
  option_find(name)->value.b = value;
}

bool option_is_int(const char *name) {
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
