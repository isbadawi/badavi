#include "options.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "editor.h"
#include "window.h"

static struct opt opts_meta[] = {
#define OPTION(name, type, _) \
  {#name, OPTION_SCOPE_BUFFER, OPTION_TYPE_##type},
  BUFFER_OPTIONS
#undef OPTION
#define OPTION(name, type, _) \
  {#name, OPTION_SCOPE_WINDOW, OPTION_TYPE_##type},
  WINDOW_OPTIONS
#undef OPTION
#define OPTION(name, type, _) \
  {#name, OPTION_SCOPE_EDITOR, OPTION_TYPE_##type},
  EDITOR_OPTIONS
#undef OPTION
  {NULL, 0, 0},
};

struct opt *option_info(char *name) {
  for (int i = 0; opts_meta[i].name; ++i) {
    if (!strcmp(opts_meta[i].name, name)) {
      return &opts_meta[i];
    }
  }
  return NULL;
}

void editor_init_options(struct editor *editor) {
#define OPTION(name, type, defaultval) \
  option_set_##type(&editor->opt.name, defaultval);
  BUFFER_OPTIONS
  EDITOR_OPTIONS
#undef OPTION
}

void window_init_options(struct window *window) {
#define OPTION(name, type, defaultval) \
  option_set_##type(&window->opt.name, defaultval);
  WINDOW_OPTIONS
#undef OPTION
}

void window_free_options(struct window *window) {
#define OPTION(name, type, _) \
  option_free_##type(window->opt.name);
  WINDOW_OPTIONS
#undef OPTION
}

void buffer_inherit_editor_options(
    struct buffer *buffer, struct editor *editor) {
#define OPTION(name, type, _) \
  option_set_##type(&buffer->opt.name, editor->opt.name);
  BUFFER_OPTIONS
#undef OPTION
}

void window_inherit_parent_options(struct window *window) {
#define OPTION(name, type, _) \
  option_set_##type(&window->opt.name, window->parent->opt.name);
  WINDOW_OPTIONS
#undef OPTION
}

void *editor_opt_val(struct editor *editor, struct opt *info, bool global) {
  switch (info->scope) {
  case OPTION_SCOPE_WINDOW:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      return &editor->window->opt.optname; \
    }
    WINDOW_OPTIONS
#undef OPTION
    break;
  case OPTION_SCOPE_BUFFER:
    if (!global) {
#define OPTION(optname, _, __) \
      if (!strcmp(info->name, #optname)) { \
        return &editor->window->buffer->opt.optname; \
      }
      BUFFER_OPTIONS
#undef OPTION
    }
    ATTR_FALLTHROUGH;
  case OPTION_SCOPE_EDITOR:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      return &editor->opt.optname; \
    }
    BUFFER_OPTIONS
    EDITOR_OPTIONS
#undef OPTION
  }

  assert(0);
  return NULL;
}

