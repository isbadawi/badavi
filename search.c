#include "search.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <regex.h>

#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "util.h"
#include "window.h"

bool editor_ignore_case(struct editor *editor, char *pattern) {
  if (!editor->opt.ignorecase) {
    return false;
  }
  if (!editor->opt.smartcase) {
    return true;
  }

  for (char *p = pattern; *p; ++p) {
    if (isupper((int) *p)) {
      return false;
    }
  }
  return true;
}

void regex_search(char *str, char *pattern, bool ignore_case, struct search_result *result) {
  regex_t regex;
  int flags = REG_EXTENDED | REG_NEWLINE;
  if (ignore_case) {
    flags |= REG_ICASE;
  }
  int err = regcomp(&regex, pattern, flags);
  if (err) {
    result->ok = false;
    regerror(err, &regex, result->error, sizeof(result->error));
    return;
  }
  result->ok = true;

  TAILQ_INIT(&result->matches);

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
      struct search_match *smatch = xmalloc(sizeof(*smatch));
      region_set(&smatch->region,
                 start + (size_t) match.rm_so,
                 start + (size_t) match.rm_eo);
      TAILQ_INSERT_TAIL(&result->matches, smatch, pointers);
      start += max(1, (size_t) match.rm_eo);
    }
  }
  regfree(&regex);
}

struct search_match *editor_search(struct editor *editor, char *pattern,
                             size_t start, enum search_direction direction) {
  bool free_pattern = false;
  if (!pattern) {
    struct editor_register *reg = editor_get_register(editor, '/');
    pattern = reg->read(reg);
    if (!*pattern) {
      editor_status_err(editor, "No previous regular expression");
      free(pattern);
      return false;
    }
    free_pattern = true;
  }

  struct gapbuf *gb = editor->window->buffer->text;
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, 0);
  struct search_result result;
  bool ignore_case = editor_ignore_case(editor, pattern);

  // FIXME(ibadawi): the gap buffer is not null-terminated in general,
  //                 but regexec expects a null-terminated string.
  char old = gb->bufend[-1];
  gb->bufend[-1] = '\0';
  regex_search(gb->gapend, pattern, ignore_case, &result);
  gb->bufend[-1] = old;

  if (!result.ok) {
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, result.error);
    return NULL;
  }

  if (TAILQ_EMPTY(&result.matches)) {
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return NULL;
  }

  struct search_match *match = NULL;
  struct search_match *m;
  if (direction == SEARCH_FORWARDS) {
    TAILQ_FOREACH(m, &result.matches, pointers) {
      if (m->region.start > start) {
        match = m;
        break;
      }
    }

    if (!match) {
      editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
      match = TAILQ_FIRST(&result.matches);
    }
  } else {
    struct search_match *last = NULL;
    TAILQ_FOREACH(m, &result.matches, pointers) {
      if (last && last->region.start < start && start <= m->region.start) {
        match = last;
        break;
      }
      last = m;
    }
    if (last->region.start < start) {
      match = last;
    }
    if (!match) {
      editor_status_msg(editor, "search hit TOP, continuing at BOTTOM");
      match = TAILQ_LAST(&result.matches, match_list);
    }
  }

  if (free_pattern) {
    free(pattern);
  }

  TAILQ_REMOVE(&result.matches, match, pointers);
  search_result_free_matches(&result);
  return match;
}

void search_result_free_matches(struct search_result *result) {
  struct search_match *m, *tm;
  TAILQ_FOREACH_SAFE(m, &result->matches, pointers, tm) {
    free(m);
  }
}

size_t match_or_default(struct search_match *match, size_t def) {
  if (match) {
    size_t start = match->region.start;
    free(match);
    return start;
  }
  return def;
}

void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction) {
  struct search_match *match = editor_search(editor, pattern, start, direction);
  window_set_cursor(editor->window,
      match_or_default(match, window_cursor(editor->window)));
}
