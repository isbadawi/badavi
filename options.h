#pragma once

#include <stdbool.h>

#include "util.h"

typedef char* string;

#define BUFFER_OPTIONS \
  OPTION(cinwords, string, "if,else,while,do,for,switch") \
  OPTION(modifiable, bool, true) \

#define WINDOW_OPTIONS \
  OPTION(numberwidth, int, 4) \
  OPTION(number, bool, false) \
  OPTION(relativenumber, bool, false) \
  OPTION(cursorline, bool, false) \

#define EDITOR_OPTIONS \
  OPTION(history, int, 50) \
  OPTION(sidescroll, int, 0) \
  OPTION(ignorecase, bool, false) \
  OPTION(smartcase, bool, false) \
  OPTION(splitright, bool, false) \
  OPTION(splitbelow, bool, false) \
  OPTION(equalalways, bool, true) \
  OPTION(hlsearch, bool, false) \
  OPTION(incsearch, bool, false) \
  OPTION(ruler, bool, false) \

struct opt {
  char *name;
  enum opt_scope {
    OPTION_SCOPE_EDITOR,
    OPTION_SCOPE_WINDOW,
    OPTION_SCOPE_BUFFER,
  } scope;
  enum opt_type {
    OPTION_TYPE_bool,
    OPTION_TYPE_int,
    OPTION_TYPE_string,
  } type;
};

static inline void option_set_int(int *p, int v) {
  *p = v;
}

static inline void option_set_bool(bool *p, bool v) {
  *p = v;
}

static inline void option_set_string(string *p, string v) {
  *p = xstrdup(v);
}

struct opt *option_info(char *name);

struct editor;
struct window;
struct buffer;

void editor_init_options(struct editor *editor);
void window_init_options(struct window *window);
void buffer_inherit_editor_options(struct buffer *buffer, struct editor *editor);
void window_inherit_parent_options(struct window *window);

void *editor_opt_val(struct editor *editor, struct opt *info, bool global);
