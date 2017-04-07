#pragma once

#include <stdbool.h>
#include <stddef.h>

struct editor;

enum search_direction {
  SEARCH_FORWARDS,
  SEARCH_BACKWARDS
};

// Search for the given pattern in the currently opened buffer, changing
// the position of the cursor. If pattern is NULL, will instead search for
// the pattern stored in the last-search-pattern register (/).
bool editor_search(struct editor *editor, char *pattern,
                   size_t start, enum search_direction direction);

struct search_result {
  char error[48];
  struct list *matches;
};

void regex_search(char *str, char *pattern, struct search_result *result);
