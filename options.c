#include "options.h"

#include <assert.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "editor.h"
#include "window.h"

static inline void option_set_int(int *p, int v) {
  *p = v;
}

static inline void option_set_bool(bool *p, bool v) {
  *p = v;
}

static inline void option_set_string(string *p, string v) {
  *p = xstrdup(v);
}

static inline void option_free_int(int i ATTR_UNUSED) {
  return;
}

static inline void option_free_bool(bool b ATTR_UNUSED) {
  return;
}

ATTR_UNUSED
static inline void option_free_string(string s) {
  free(s);
}

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

static struct opt *option_info(char *name) {
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

  if (buffer->directory) {
    buffer->opt.readonly = true;
    buffer->opt.modifiable = false;
  }
}

void window_inherit_parent_options(struct window *window) {
#define OPTION(name, type, _) \
  option_set_##type(&window->opt.name, window->parent->opt.name);
  WINDOW_OPTIONS
#undef OPTION
}

static void *editor_opt_val(struct editor *editor, struct opt *info, bool global) {
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

static void editor_command_set_impl(struct editor *editor, char *arg,
                                    bool global) {
  if (!arg) {
    // TODO(isbadawi): show current values of all options...
    editor_status_err(editor, "Argument required");
    return;
  }

  regex_t regex;
  regcomp(&regex, "(no)?([a-z]+)(=[0-9a-zA-Z,_]+|!|\\?)?", REG_EXTENDED);

  regmatch_t groups[4];
  int nomatch = regexec(&regex, arg, 4, groups, 0);
  regfree(&regex);

  if (nomatch) {
    editor_status_err(editor, "Invalid argument: %s", arg);
    return;
  }

  char opt[32];
  size_t optlen = (size_t) (groups[2].rm_eo - groups[2].rm_so);
  strncpy(opt, arg + groups[2].rm_so, optlen);
  opt[optlen] = '\0';

  struct opt *info = option_info(opt);

  if (!info) {
    editor_status_err(editor, "Unknown option: %s", opt);
    return;
  }

  void *val = editor_opt_val(editor, info, global);
  bool *boolval = (bool*)val;
  int *intval = (int*)val;
  char **stringval = (char**)val;

  if (groups[3].rm_so == -1) {
    switch (info->type) {
    case OPTION_TYPE_int:
      editor_status_msg(editor, "%s=%d", opt, *intval);
      break;
    case OPTION_TYPE_string:
      editor_status_msg(editor, "%s=%s", opt, *stringval);
      break;
    case OPTION_TYPE_bool:
      *boolval = groups[1].rm_so == -1;
      break;
    }
    return;
  }

  switch (arg[groups[3].rm_so]) {
  case '!':
    switch (info->type) {
    case OPTION_TYPE_bool:
      *boolval = !*boolval;
      break;
    case OPTION_TYPE_int:
    case OPTION_TYPE_string:
      editor_status_err(editor, "Invalid argument: %s", arg);
      break;
    }
    break;
  case '?':
    switch (info->type) {
    case OPTION_TYPE_int:
      editor_status_msg(editor, "%s=%d", opt, *intval);
      break;
    case OPTION_TYPE_string:
      editor_status_msg(editor, "%s=%s", opt, *stringval);
      break;
    case OPTION_TYPE_bool:
      editor_status_msg(editor, "%s%s", *boolval ? "" : "no", opt);
      break;
    }
    break;
  case '=':
    switch (info->type) {
    case OPTION_TYPE_int:
      strtoi(arg + groups[3].rm_so + 1, intval);
      break;
    case OPTION_TYPE_string:
      free(*stringval);
      option_set_string(stringval, arg + groups[3].rm_so + 1);
      break;
    case OPTION_TYPE_bool:
      editor_status_err(editor, "Invalid argument: %s", arg);
      break;
    }
    break;
  }
  return;
}

EDITOR_COMMAND(setglobal, setg) {
  editor_command_set_impl(editor, arg, true);
}

EDITOR_COMMAND(setlocal, setl) {
  editor_command_set_impl(editor, arg, false);
}

EDITOR_COMMAND(set, set) {
  editor_command_set_impl(editor, arg, true);
  editor_command_set_impl(editor, arg, false);
}
