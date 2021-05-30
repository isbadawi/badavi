#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <sys/queue.h>

#include "util.h"

struct editor;

enum search_direction {
  SEARCH_FORWARDS,
  SEARCH_BACKWARDS
};

struct search_match {
  struct region region;
  TAILQ_ENTRY(search_match) pointers;
};

// Search for the given pattern in the currently opened buffer, returning the
// first match. If pattern is NULL, will instead search for the pattern stored
// in the last-search-pattern register (/).
struct search_match *editor_search(struct editor *editor, char *pattern,
                             size_t start, enum search_direction direction);

struct search_result {
  bool ok;
  union {
    char error[48];
    TAILQ_HEAD(match_list, search_match) matches;
  };
};

void search_result_free_matches(struct search_result *result);

bool editor_ignore_case(struct editor *editor, char *pattern);

void regex_search(char *str, size_t len, char *pattern, bool ignore_case,
                  struct search_result *result);

size_t match_or_default(struct search_match *match, size_t def);
void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction);
