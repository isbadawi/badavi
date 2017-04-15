#pragma once

#include <stdbool.h>
#include <stddef.h>

struct editor;

enum search_direction {
  SEARCH_FORWARDS,
  SEARCH_BACKWARDS
};

// Search for the given pattern in the currently opened buffer, returning the
// first match. If pattern is NULL, will instead search for the pattern stored
// in the last-search-pattern register (/).
struct region *editor_search(struct editor *editor, char *pattern,
                             size_t start, enum search_direction direction);

struct search_result {
  char error[48];
  struct list *matches;
};

bool editor_ignore_case(struct editor *editor, char *pattern);

void regex_search(char *str, char *pattern, bool ignore_case,
                  struct search_result *result);

size_t match_or_default(struct region *match, size_t def);
void editor_jump_to_match(struct editor *editor, char *pattern,
                          size_t start, enum search_direction direction);
