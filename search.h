#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <pcre2.h>

#include "util.h"

struct editor;

enum search_direction {
  SEARCH_FORWARDS,
  SEARCH_BACKWARDS
};

struct search {
  pcre2_code *regex;
  pcre2_match_data *groups;
  unsigned char *str;
  size_t len;
  size_t start;
};

int search_init(struct search *search, char *pattern, bool ignore_case,
    char *str, size_t len);
void search_get_error(int error, char *buf, size_t buflen);
void search_deinit(struct search *search);
bool search_next_match(struct search *search, struct region *match);

// Search for the given pattern in the currently opened buffer, returning the
// first match. If pattern is NULL, will instead search for the pattern stored
// in the last-search-pattern register (/).
bool editor_search(
    struct editor *editor, char *pattern,
    size_t start, enum search_direction direction, struct region *match);

bool editor_ignore_case(struct editor *editor, char *pattern);

void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction);
