#include "options.h"

#include <stdlib.h>
#include <string.h>

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
