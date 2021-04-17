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

static int pstrcmp(const void *a, const void *b) {
  return strcmp(*(char * const *)a, *(char * const *)b);
}

char **options_get_sorted(int *len) {
  int num_options = 0;
#define OPTION(_, __, ___) num_options++;
  BUFFER_OPTIONS
  WINDOW_OPTIONS
  EDITOR_OPTIONS
#undef OPTION

  char **options = xmalloc(sizeof(*options) * num_options);
  int i = 0;
#define OPTION(name, _, __) options[i++] = #name;
  BUFFER_OPTIONS
  WINDOW_OPTIONS
  EDITOR_OPTIONS
#undef OPTION

  qsort(options, num_options, sizeof(*options), pstrcmp);

  *len = num_options;
  return options;
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

static bool csl_has(char *csl, char *word) {
  char *p, *words;
  p = words = xstrdup(csl);

  char *token;
  while ((token = strsep(&words, ","))) {
    if (!strcmp(token, word)) {
      free(p);
      return true;
    }
  }
  free(p);
  return false;
}

static void csl_append(char **csl, char *word) {
  size_t len = strlen(*csl);
  size_t wordlen = strlen(word);
  char *result = xmalloc(len + wordlen + 2);
  *result = '\0';
  if (len) {
    strcat(result, *csl);
    if ((*csl)[len - 1] != ',') {
      strcat(result, ",");
    }
  }
  strcat(result, word);
  free(*csl);
  *csl = result;
}

static void editor_command_set_impl(
    struct editor *editor, char *arg, enum option_set_mode which) {
  if (!arg) {
    // TODO(isbadawi): show current values of all options...
    editor_status_err(editor, "Argument required");
    return;
  }

  regex_t regex;
  regcomp(&regex, "(no)?([a-z]+)(\\+?=[0-9a-zA-Z,_]*|!|\\?|&)?", REG_EXTENDED);

  regmatch_t groups[4];
  int nomatch = regexec(&regex, arg, 4, groups, 0);
  regfree(&regex);

#define ENSURE(condition, fmt, ...) \
  if (!(condition)) { \
    editor_status_err(editor, fmt, ##__VA_ARGS__); \
    return; \
  }

#define INVALID(condition) ENSURE(!(condition), "Invalid argument: %s", arg)

  INVALID(nomatch);

  char opt[32];
  size_t optlen = (size_t) (groups[2].rm_eo - groups[2].rm_so);
  strncpy(opt, arg + groups[2].rm_so, optlen);
  opt[optlen] = '\0';

  struct opt *info = option_info(opt);
  ENSURE(info, "Unknown option: %s", opt);

  enum {
    OPTION_ACTION_NONE,
    OPTION_ACTION_SHOW,
    OPTION_ACTION_ENABLE,
    OPTION_ACTION_DISABLE,
    OPTION_ACTION_TOGGLE,
    OPTION_ACTION_RESET,
    OPTION_ACTION_ASSIGN,
    OPTION_ACTION_ASSIGN_ADD,
  } action = OPTION_ACTION_NONE;
  char *rhs = NULL;

  regoff_t rhs_offset = groups[3].rm_so;
  regoff_t no_offset = groups[1].rm_so;
  INVALID(no_offset != -1 && info->type != OPTION_TYPE_bool);

  if (rhs_offset == -1) {
    switch (info->type) {
    case OPTION_TYPE_int:
    case OPTION_TYPE_string:
      action = OPTION_ACTION_SHOW;
      break;
    case OPTION_TYPE_bool:
      if (no_offset == -1) {
        action = OPTION_ACTION_ENABLE;
      } else {
        action = OPTION_ACTION_DISABLE;
      }
    }
  } else {
    switch (arg[rhs_offset]) {
    case '!':
      INVALID(info->type != OPTION_TYPE_bool);
      action = OPTION_ACTION_TOGGLE;
      break;
    case '?': action = OPTION_ACTION_SHOW; break;
    case '&': action = OPTION_ACTION_RESET; break;
    case '+':
      INVALID(info->type == OPTION_TYPE_bool);
      action = OPTION_ACTION_ASSIGN_ADD;
      rhs = arg + rhs_offset + 2;
      break;
    case '=':
      INVALID(info->type == OPTION_TYPE_bool);
      action = OPTION_ACTION_ASSIGN;
      rhs = arg + rhs_offset + 1;
      break;
    }
  }

  void *global, *local;
  editor_opt_vals(editor, info, &global, &local);
  assert(global == local || info->scope == OPTION_SCOPE_BUFFER);

  void *writes[2 + 1] = {0};
  if (info->scope == OPTION_SCOPE_BUFFER && which == OPTION_SET_BOTH) {
    writes[0] = global;
    writes[1] = local;
  } else {
    writes[0] = which == OPTION_SET_GLOBAL ? global : local;
  }
  #define WRITE(val) for (void **_ = writes, *val = *_; val; ++_, val = *_)

  switch (action) {
  case OPTION_ACTION_NONE: assert(0); break;
  case OPTION_ACTION_SHOW: {
    void *val = which == OPTION_SET_GLOBAL ? global : local;
    switch (info->type) {
    case OPTION_TYPE_int:
      editor_status_msg(editor, "%s=%d", opt, *(int*)val);
      break;
    case OPTION_TYPE_string:
      editor_status_msg(editor, "%s=%s", opt, *(string*)val);
      break;
    case OPTION_TYPE_bool:
      editor_status_msg(editor, "%s%s", *(bool*)val ? "" : "no", opt);
      break;
    }
    break;
  }
  case OPTION_ACTION_ENABLE:
    assert(info->type == OPTION_TYPE_bool);
    WRITE(val) { *(bool*)val = true; } break;
  case OPTION_ACTION_DISABLE:
    assert(info->type == OPTION_TYPE_bool);
    WRITE(val) { *(bool*)val = false; } break;
  case OPTION_ACTION_TOGGLE:
    assert(info->type == OPTION_TYPE_bool);
    WRITE(val) { *(bool*)val = !*(bool*)val; } break;
  case OPTION_ACTION_RESET:
    switch (info->type) {
    case OPTION_TYPE_int:
      WRITE(val) {
        *(int*)val = info->defaultval.intval;
      }
      break;
    case OPTION_TYPE_bool:
      WRITE(val) {
        *(bool*)val = info->defaultval.boolval;
      }
      break;
    case OPTION_TYPE_string:
      WRITE(val) {
        free(*(string*)val);
        option_set_string((string*)val, info->defaultval.stringval);
      }
      break;
    }
    break;
  case OPTION_ACTION_ASSIGN:
    assert(rhs);
    switch (info->type) {
    case OPTION_TYPE_bool: assert(0); break;
    case OPTION_TYPE_int:
      WRITE(val) {
        ENSURE(strtoi(rhs, (int*)val), "Number required after =: %s", arg);
      }
      break;
    case OPTION_TYPE_string:
      WRITE(val) {
        free(*(string*)val);
        option_set_string((string*)val, rhs);
      }
    }
    break;
  case OPTION_ACTION_ASSIGN_ADD:
    assert(rhs);
    switch (info->type) {
    case OPTION_TYPE_bool: assert(0); break;
    case OPTION_TYPE_int:
      WRITE(val) {
        int increment;
        ENSURE(strtoi(rhs, &increment), "Number required after =: %s", arg);
        *(int*)val += increment;
      }
      break;
    case OPTION_TYPE_string:
      WRITE(val) {
        if (csl_has(*(string*)val, rhs)) {
          continue;
        }
        csl_append((string*)val, rhs);
      }
      break;
    }
  }
}

EDITOR_COMMAND_WITH_COMPLETION(setglobal, setg, COMPLETION_OPTIONS) {
  editor_command_set_impl(editor, arg, OPTION_SET_GLOBAL);
}

EDITOR_COMMAND_WITH_COMPLETION(setlocal, setl, COMPLETION_OPTIONS) {
  editor_command_set_impl(editor, arg, OPTION_SET_LOCAL);
}

EDITOR_COMMAND_WITH_COMPLETION(set, set, COMPLETION_OPTIONS) {
  editor_command_set_impl(editor, arg, OPTION_SET_BOTH);
}
