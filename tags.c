#include "tags.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tags_t *tags_create(void) {
  tags_t *tags = malloc(sizeof(*tags));
  tags->tags = NULL;
  tags->len = 0;
  return tags;
}


static char *escape_regex(char *regex) {
  // Worst case, every char is escaped...
  size_t len = strlen(regex);
  char *result = malloc(len * 2 + 1);
  char *dest = result;
  for (char *src = regex; *src; ++src) {
    switch (*src) {
    case '*': case '+': case '(': case ')':
      *dest++ = '\\';
      // fallthrough
    default:
      *dest++ = *src;
    }
  }
  *dest = '\0';
  return result;
}

void tags_load(tags_t *tags, char *file) {
  FILE *fp = fopen(file, "r");
  if (!fp) {
    return;
  }

  size_t nlines = 0;
  size_t n = 0;
  char *line = NULL;
  ssize_t len = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    if (*line != '!') {
      nlines++;
    }
  }
  fseek(fp, 0, SEEK_SET);

  tags->len = nlines;
  tags->tags = malloc(sizeof(*tags->tags) * nlines);

  size_t i = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    line[len - 1] = '\0';
    if (*line == '!') {
      continue;
    }
    tag_t *tag = &tags->tags[i++];
    tag->name = strdup(strtok(line, "\t"));
    tag->path = strdup(strtok(NULL, "\t"));
    tag->cmd = escape_regex(strtok(NULL, "\""));
    // TODO(isbadawi): Hack since we don't have ex mode...
    size_t cmdlen = strlen(tag->cmd);
    tag->cmd = realloc(tag->cmd, cmdlen + 3);
    tag->cmd[cmdlen - 2] = '\0';
    strcat(tag->cmd, "<cr>");
  }
  free(line);
  fclose(fp);
}

static int tag_compare(const void *lhs, const void *rhs) {
  return strcmp((const char*) lhs, ((const tag_t*) rhs)->name);
}

tag_t *tags_find(tags_t *tags, char *name) {
  return bsearch(name, tags->tags, tags->len, sizeof(tag_t), tag_compare);
}
