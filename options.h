#pragma once

#include <stdbool.h>

typedef char* string;

#define BUFFER_OPTIONS \
  OPTION(autoindent, bool, false) \
  OPTION(smartindent, bool, false) \
  OPTION(shiftwidth, int, 8) \
  OPTION(tabstop, int, 8) \
  OPTION(expandtab, bool, false) \
  OPTION(cinwords, string, "if,else,while,do,for,switch") \
  OPTION(modified, bool, false) \
  OPTION(modifiable, bool, true) \
  OPTION(readonly, bool, false) \

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

struct editor;
struct window;
struct buffer;

void editor_init_options(struct editor *editor);
void window_init_options(struct window *window);
void window_free_options(struct window *window);
void buffer_inherit_editor_options(struct buffer *buffer, struct editor *editor);
void window_inherit_parent_options(struct window *window);
