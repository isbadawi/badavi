#include "search.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
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

int search_init(struct search *search, char *pattern, bool ignore_case,
    char *str, size_t len) {
  memset(search, 0, sizeof(*search));

  int flags = PCRE2_MULTILINE;
  if (ignore_case) {
    flags |= PCRE2_CASELESS;
  }
  int errorcode = 0;
  PCRE2_SIZE erroroffset = 0;
  search->regex = pcre2_compile(
      (unsigned char*) pattern, PCRE2_ZERO_TERMINATED,
      flags, &errorcode, &erroroffset, NULL);
  if (!search->regex) {
    assert(errorcode != 0);
    return errorcode;
  }
  search->groups = pcre2_match_data_create_from_pattern(search->regex, NULL);

  search->str = (unsigned char*)str;
  search->len = len;
  search->start = 0;
  return 0;
}

void search_deinit(struct search *search) {
  pcre2_code_free(search->regex);
  pcre2_match_data_free(search->groups);
}

void search_get_error(int error, char *buf, size_t buflen) {
  pcre2_get_error_message(error, (unsigned char*)buf, buflen);
}

bool search_next_match(struct search *search, struct region *match) {
  // In multiline mode, pcre2 assumes the string is at the beginning of a
  // line unless told otherwise. This affects patterns that use ^.
  int flags = 0;
  if (search->start > 0 && search->str[search->start - 1] != '\n') {
    flags |= PCRE2_NOTBOL;
  }

  int rc = pcre2_match(
      search->regex, search->str, search->len,
      search->start, flags, search->groups, NULL);

  if (rc > 0) {
    PCRE2_SIZE *offsets = pcre2_get_ovector_pointer(search->groups);
    region_set(match, offsets[0], offsets[1]);
    search->start += max(1, offsets[1] - search->start);
    return true;
  }
  return false;
}

bool editor_search(struct editor *editor, char *pattern,
    size_t start, enum search_direction direction, struct region *match) {
  if (!pattern) {
    struct editor_register *reg = editor_get_register(editor, '/');
    assert(reg->buf);
    pattern = reg->buf->buf;
    if (!*pattern) {
      editor_status_err(editor, "No previous regular expression");
      return NULL;
    }
  }

  struct gapbuf *gb = editor->window->buffer->text;
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, 0);
  bool ignore_case = editor_ignore_case(editor, pattern);

  struct search search;
  int rc = search_init(&search, pattern, ignore_case, gb->gapend, gb_size(gb));

  if (rc) {
    char error[48];
    search_get_error(rc, error, sizeof(error));
    editor_status_err(editor, "Bad regex \"%s\": %s", pattern, error);
    return false;
  }

#define return search_deinit(&search); return

  if (direction == SEARCH_FORWARDS) {
    search.start = start + 1;
    if (search_next_match(&search, match)) {
      return true;
    }

    editor_status_msg(editor, "search hit BOTTOM, continuing at TOP");
    search.start = 0;
    if (search_next_match(&search, match)) {
      return true;
    }

    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return false;
  }

  assert(direction == SEARCH_BACKWARDS);

  if (!search_next_match(&search, match)) {
    editor_status_err(editor, "Pattern not found: \"%s\"", pattern);
    return false;
  }

  if (match->start >= start) {
    editor_status_msg(editor, "search hit TOP, continuing at BOTTOM");
    while (search_next_match(&search, match)) {}
    return true;
  }

  struct region prev = *match;
  while (match->start < start) {
    prev = *match;
    if (!search_next_match(&search, match)) {
      return true;
    }
  }
  *match = prev;
  return true;
#undef return
}

void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction) {
  struct region match;
  if (editor_search(editor, pattern, start, direction, &match)) {
    window_set_cursor(editor->window, match.start);
  } else {
    window_set_cursor(editor->window, window_cursor(editor->window));
  }
}
