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

struct match_t {
  size_t start;
  size_t len;
};

struct search_result_t {
  char error[48];
  struct list_t *matches;
};

static void gb_search(struct gapbuf_t *gb, char *pattern,
                      struct search_result_t *result) {
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, 0);

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
    if (start > 0 && gb_getchar(gb, start - 1) != '\n') {
      flags |= REG_NOTBOL;
    }

    nomatch = regexec(&regex, gb->gapend + start, 1, &match, flags);
    if (!nomatch) {
      struct match_t *region = xmalloc(sizeof(*region));
      region->start = start + (size_t) match.rm_so;
      region->len = (size_t) (match.rm_eo - match.rm_so);
      list_append(result->matches, region);
      start += max(1, (size_t) match.rm_eo);
    }
  }
  regfree(&regex);
}

// TODO(isbadawi): Searching should be a motion.
void editor_search(struct editor_t *editor, char *pattern,
                   enum search_direction_t direction) {
  if (!pattern) {
    struct buf_t *reg = editor_get_register(editor, '/');
    if (reg->len == 0) {
      editor_status_err(editor, "No previous regular expression");
      return;
    }
    pattern = reg->buf;
  }

  struct gapbuf_t *gb = editor->window->buffer->text;
  struct search_result_t result;
  gb_search(gb, pattern, &result);

  if (!result.matches) {
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, result.error);
    return;
  }

  if (list_empty(result.matches)) {
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return;
  }

  struct match_t *match = NULL;
  struct match_t *m;
  if (direction == SEARCH_FORWARDS) {
    LIST_FOREACH(result.matches, m) {
      if (m->start > editor->window->cursor) {
        match = m;
        break;
      }
    }

    if (!match) {
      editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
      match = result.matches->head->next->data;
    }
  } else {
    struct match_t *last = NULL;
    LIST_FOREACH(result.matches, m) {
      if (last && last->start < editor->window->cursor &&
          m->start >= editor->window->cursor) {
        match = last;
        break;
      }
      last = m;
    }
    if (last->start < editor->window->cursor) {
      match = last;
    }
    if (!match) {
      editor_status_msg(editor, "search hit TOP, continuing at BOTTOM");
      match = result.matches->tail->prev->data;
    }
  }

  editor->window->cursor = match->start;

  // TODO(isbadawi): Remember matches -- can do hlsearch, or cache searches
  // if the buffer hasn't changed.
  LIST_FOREACH(result.matches, m) {
    free(m);
  }
  list_clear(result.matches);
}
