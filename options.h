#pragma once

#include <stdbool.h>

#define BUFFER_OPTIONS \
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
  } type;
};

struct opt *option_info(char *name);
