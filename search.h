#pragma once

struct editor_t;

enum search_direction_t {
  SEARCH_FORWARDS,
  SEARCH_BACKWARDS
};

// Search for the given pattern in the currently opened buffer, changing
// the position of the cursor. If pattern is NULL, will instead search for
// the pattern stored in the last-search-pattern register (/).
void editor_search(struct editor_t *editor, char *pattern,
                   enum search_direction_t direction);
