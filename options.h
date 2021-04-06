#pragma once

#include <stdbool.h>

typedef char* string;

#define BUFFER_OPTIONS \
  OPTION(autoindent, bool, false) \
  OPTION(cinwords, string, "if,else,while,do,for,switch") \
  OPTION(expandtab, bool, false) \
  OPTION(modifiable, bool, true) \
  OPTION(modified, bool, false) \
  OPTION(readonly, bool, false) \
  OPTION(shiftwidth, int, 8) \
  OPTION(smartindent, bool, false) \
  OPTION(suffixesadd, string, "") \
  OPTION(tabstop, int, 8) \

#define WINDOW_OPTIONS \
  OPTION(cursorline, bool, false) \
  OPTION(number, bool, false) \
  OPTION(numberwidth, int, 4) \
  OPTION(relativenumber, bool, false) \

#define EDITOR_OPTIONS \
  OPTION(equalalways, bool, true) \
  OPTION(history, int, 50) \
  OPTION(hlsearch, bool, false) \
  OPTION(ignorecase, bool, false) \
  OPTION(incsearch, bool, false) \
  OPTION(path, string, ".,/usr/include,,") \
  OPTION(ruler, bool, false) \
  OPTION(showmode, bool, true) \
  OPTION(sidescroll, int, 0) \
  OPTION(smartcase, bool, false) \
  OPTION(splitbelow, bool, false) \
  OPTION(splitright, bool, false) \

struct editor;
struct window;
struct buffer;

void editor_init_options(struct editor *editor);
void window_init_options(struct window *window);
void window_free_options(struct window *window);
void buffer_inherit_editor_options(struct buffer *buffer, struct editor *editor);
void window_inherit_parent_options(struct window *window);
