#include "search.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <regex.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "list.h"
#include "options.h"
#include "util.h"
#include "window.h"

static bool search_should_ignore_case(char *pattern) {
  if (!option_get_bool("ignorecase")) {
    return false;
  }
  if (!option_get_bool("smartcase")) {
    return true;
  }

  for (char *p = pattern; *p; ++p) {
    if (isupper((int) *p)) {
      return false;
    }
  }
  return true;
}

void regex_search(char *str, char *pattern, struct search_result *result) {
  regex_t regex;
  int flags = REG_EXTENDED | REG_NEWLINE;
  if (search_should_ignore_case(pattern)) {
    flags |= REG_ICASE;
  }
  int err = regcomp(&regex, pattern, flags);
  if (err) {
    result->matches = NULL;
    regerror(err, &regex, result->error, sizeof(result->error));
    return;
  }

  result->matches = list_create();

  int nomatch = 0;
  size_t start = 0;
  while (!nomatch) {
    regmatch_t match;
    // regexec assumes the string is at the beginning of a line unless told
    // otherwise. This affects regexes that use ^.
    flags = 0;
    if (start > 0 && str[start - 1] != '\n') {
      flags |= REG_NOTBOL;
    }

    nomatch = regexec(&regex, str + start, 1, &match, flags);
    if (!nomatch) {
      struct region *region = region_create(
          start + (size_t) match.rm_so, start + (size_t) match.rm_eo);
      list_append(result->matches, region);
      start += max(1, (size_t) match.rm_eo);
    }
  }
  regfree(&regex);
}

struct region *editor_search(struct editor *editor, char *pattern,
                             size_t start, enum search_direction direction) {
  if (!pattern) {
    struct buf *reg = editor_get_register(editor, '/');
    if (reg->len == 0) {
      editor_status_err(editor, "No previous regular expression");
      return false;
    }
    pattern = reg->buf;
  }

  struct gapbuf *gb = editor->window->buffer->text;
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, 0);
  struct search_result result;
  regex_search(gb->gapend, pattern, &result);

  if (!result.matches) {
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, result.error);
    return NULL;
  }

  if (list_empty(result.matches)) {
    list_free(result.matches, free);
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return NULL;
  }

  struct region *match = NULL;
  struct region *m;
  if (direction == SEARCH_FORWARDS) {
    LIST_FOREACH(result.matches, m) {
      if (m->start > start) {
        match = m;
        break;
      }
    }

    if (!match) {
      editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
      match = list_first(result.matches);
    }
  } else {
    struct region *last = NULL;
    LIST_FOREACH(result.matches, m) {
      if (last && last->start < start && start <= m->start) {
        match = last;
        break;
      }
      last = m;
    }
    if (last->start < start) {
      match = last;
    }
    if (!match) {
      editor_status_msg(editor, "search hit TOP, continuing at BOTTOM");
      match = list_last(result.matches);
    }
  }

  list_remove(result.matches, match);
  list_free(result.matches, free);
  return match;
}

size_t match_or_default(struct region *match, size_t def) {
  if (match) {
    size_t start = match->start;
    free(match);
    return start;
  }
  return def;
}

void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction) {
  struct region *match = editor_search(editor, pattern, start, direction);
  window_set_cursor(editor->window,
      match_or_default(match, window_cursor(editor->window)));
}
