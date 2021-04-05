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
  union {
    int intval;
    bool boolval;
    string stringval;
  } defaultval;
};

static struct opt opts_meta[] = {
#define OPTION(name, type, defaultval) \
  {#name, OPTION_SCOPE_BUFFER, OPTION_TYPE_##type, {.type##val = defaultval}},
  BUFFER_OPTIONS
#undef OPTION
#define OPTION(name, type, defaultval) \
  {#name, OPTION_SCOPE_WINDOW, OPTION_TYPE_##type, {.type##val = defaultval}},
  WINDOW_OPTIONS
#undef OPTION
#define OPTION(name, type, defaultval) \
  {#name, OPTION_SCOPE_EDITOR, OPTION_TYPE_##type, {.type##val = defaultval}},
  EDITOR_OPTIONS
#undef OPTION
  {NULL, 0, 0, {0}},
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

static void editor_opt_vals(
    struct editor *editor, struct opt *info, void **global, void **local) {
  switch (info->scope) {
  case OPTION_SCOPE_WINDOW:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      *global = *local = &editor->window->opt.optname; \
      return; \
    }
    WINDOW_OPTIONS
#undef OPTION
    break;
  case OPTION_SCOPE_BUFFER:
#define OPTION(optname, _, __) \
      if (!strcmp(info->name, #optname)) { \
        *global = &editor->opt.optname; \
        *local = &editor->window->buffer->opt.optname; \
        return; \
      }
      BUFFER_OPTIONS
#undef OPTION
      break;
  case OPTION_SCOPE_EDITOR:
#define OPTION(optname, _, __) \
    if (!strcmp(info->name, #optname)) { \
      *global = *local = &editor->opt.optname; \
      return; \
    }
    BUFFER_OPTIONS
    EDITOR_OPTIONS
#undef OPTION
  }

  assert(0);
}

enum option_set_mode {
  OPTION_SET_LOCAL,
  OPTION_SET_GLOBAL,
  OPTION_SET_BOTH
};

static void editor_command_set_impl(
    struct editor *editor, char *arg, enum option_set_mode which) {
  if (!arg) {
    // TODO(isbadawi): show current values of all options...
    editor_status_err(editor, "Argument required");
    return;
  }

  regex_t regex;
  regcomp(&regex, "(no)?([a-z]+)(=[0-9a-zA-Z,_]+|!|\\?|&)?", REG_EXTENDED);

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

  void *global, *local;
  editor_opt_vals(editor, info, &global, &local);
  assert(global == local || info->scope == OPTION_SCOPE_BUFFER);

  void *read = which == OPTION_SET_GLOBAL ? global : local;
  void *writes[2 + 1] = {0};

  writes[0] = read;

  if (info->scope == OPTION_SCOPE_BUFFER && which == OPTION_SET_BOTH) {
    writes[1] = writes[0] == global ? local : global;
  }

  #define WRITE(val) for (void **_ = writes, *val = *_; val; ++_, val = *_)

  if (groups[3].rm_so == -1) {
    switch (info->type) {
    case OPTION_TYPE_int:
      editor_status_msg(editor, "%s=%d", opt, *(int*)read);
      break;
    case OPTION_TYPE_string:
      editor_status_msg(editor, "%s=%s", opt, *(string*)read);
      break;
    case OPTION_TYPE_bool:
      WRITE(val) {
        *(bool*)val = groups[1].rm_so == -1;
      }
      break;
    }
    return;
  }

  switch (arg[groups[3].rm_so]) {
  case '!':
    switch (info->type) {
    case OPTION_TYPE_bool:
      WRITE(val) {
        *(bool*)val = !*(bool*)val;
      }
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
      editor_status_msg(editor, "%s=%d", opt, *(int*)read);
      break;
    case OPTION_TYPE_string:
      editor_status_msg(editor, "%s=%s", opt, *(string*)read);
      break;
    case OPTION_TYPE_bool:
      editor_status_msg(editor, "%s%s", *(bool*)read ? "" : "no", opt);
      break;
    }
    break;
  case '&':
    switch (info->type) {
    case OPTION_TYPE_int:
      WRITE(val) {
        *(int*)val = info->defaultval.intval;
      }
      break;
    case OPTION_TYPE_string:
      WRITE(val) {
        free(*(string*)val);
        option_set_string((string*)val, info->defaultval.stringval);
      }
      break;
    case OPTION_TYPE_bool:
      WRITE(val) {
        *(bool*)val = info->defaultval.boolval;
      }
      break;
    }
    break;
  case '=':
    switch (info->type) {
    case OPTION_TYPE_int:
      WRITE(val) {
        strtoi(arg + groups[3].rm_so + 1, (int*)val);
      }
      break;
    case OPTION_TYPE_string:
      WRITE(val) {
        free(*(string*)val);
        option_set_string((string*)val, arg + groups[3].rm_so + 1);
      }
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
  editor_command_set_impl(editor, arg, OPTION_SET_GLOBAL);
}

EDITOR_COMMAND(setlocal, setl) {
  editor_command_set_impl(editor, arg, OPTION_SET_LOCAL);
}

EDITOR_COMMAND(set, set) {
  editor_command_set_impl(editor, arg, OPTION_SET_BOTH);
}
