#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

static char *escape_regex(char *regex) {
  // Worst case, every char is escaped...
  size_t len = strlen(regex);
  char *result = malloc(len * 2 + 1);
  char *dest = result;
  for (char *src = regex; *src; ++src) {
    switch (*src) {
    case '*': case '+': case '(': case ')': case '[': case ']':
      *dest++ = '\\';
      // fallthrough
    default:
      *dest++ = *src;
    }
  }
  *dest = '\0';
  return result;
}

static void tags_clear(struct tags_t *tags) {
  for (size_t i = 0; i < tags->len; ++i) {
    struct tag_t *tag = &tags->tags[i];
    free(tag->name);
    free(tag->path);
    free(tag->cmd);
  }
  if (tags->len) {
    free(tags->tags);
  }
  tags->tags = NULL;
  tags->len = 0;
  tags->loaded_at = 0;
}

static void tags_load(struct tags_t *tags) {
  FILE *fp = fopen(tags->file, "r");
  if (!fp) {
    return;
  }

  tags->loaded_at = time(0);

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

  if (tags->len) {
    tags_clear(tags);
  }
  tags->tags = malloc(sizeof(*tags->tags) * nlines);
  tags->len = nlines;

  size_t i = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    line[len - 1] = '\0';
    if (*line == '!') {
      continue;
    }
    struct tag_t *tag = &tags->tags[i++];
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

struct tags_t *tags_create(char *file) {
  struct tags_t *tags = malloc(sizeof(*tags));
  tags_clear(tags);
  tags->file = file;

  tags_load(tags);
  return tags;
}

static int tag_compare(const void *lhs, const void *rhs) {
  return strcmp((const char*) lhs, ((const struct tag_t*) rhs)->name);
}

struct tag_t *tags_find(struct tags_t *tags, char *name) {
  struct stat info;
  int err = stat(tags->file, &info);
  if (err) {
    tags_clear(tags);
    return NULL;
  }

  if (difftime(info.st_mtime, tags->loaded_at) > 0) {
    tags_load(tags);
  }

  return bsearch(name, tags->tags, tags->len, sizeof(struct tag_t), tag_compare);
}
