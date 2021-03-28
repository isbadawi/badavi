#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "buf.h"
#include "editor.h"
#include "search.h"
#include "util.h"
#include "window.h"

static char *escape_regex(char *regex) {
  // Worst case, every char is escaped...
  size_t len = strlen(regex);
  char *result = xmalloc(len * 2 + 1);
  char *dest = result;
  for (char *src = regex; *src; ++src) {
    if (strchr("*+()[]{}", *src)) {
      *dest++ = '\\';
    }
    *dest++ = *src;
  }
  *dest = '\0';
  return result;
}

void tags_clear(struct tags *tags) {
  for (size_t i = 0; i < tags->len; ++i) {
    struct tag *tag = &tags->tags[i];
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

static void tags_load(struct tags *tags) {
  FILE *fp = fopen(tags->file, "r");
  if (!fp) {
    return;
  }

  size_t nlines = 0;
  size_t n = 0;
  char *line = NULL;
  while (getline(&line, &n, fp) != -1) {
    if (*line != '!') {
      nlines++;
    }
  }
  fseek(fp, 0, SEEK_SET);

  if (tags->len) {
    tags_clear(tags);
  }
  tags->tags = xmalloc(sizeof(*tags->tags) * nlines);
  tags->len = nlines;

  size_t i = 0;
  ssize_t len = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    line[len - 1] = '\0';
    if (*line == '!') {
      continue;
    }
    struct tag *tag = &tags->tags[i++];
    tag->name = xstrdup(strtok(line, "\t"));
    tag->path = xstrdup(strtok(NULL, "\t"));
    tag->cmd = escape_regex(strtok(NULL, "\""));
    tag->cmd[strlen(tag->cmd) - 2] = '\0';
  }
  free(line);
  fclose(fp);

  tags->loaded_at = time(0);
}

struct tags *tags_create(const char *file) {
  struct tags *tags = xmalloc(sizeof(*tags));
  tags->file = file;
  tags->tags = NULL;
  tags->len = 0;
  tags->loaded_at = 0;

  tags_load(tags);
  return tags;
}

static int tag_compare(const void *lhs, const void *rhs) {
  return strcmp((const char*) lhs, ((const struct tag*) rhs)->name);
}

struct tag *tags_find(struct tags *tags, char *name) {
  struct stat info;
  int err = stat(tags->file, &info);
  if (err) {
    tags_clear(tags);
    return NULL;
  }

  if (difftime(info.st_mtime, tags->loaded_at) > 0) {
    tags_load(tags);
  }

  return bsearch(name, tags->tags, tags->len, sizeof(struct tag), tag_compare);
}

void editor_jump_to_tag(struct editor *editor, char *name) {
  struct tag *tag = tags_find(editor->tags, name);
  if (!tag) {
    editor_status_err(editor, "tag not found: %s", name);
    return;
  }

  struct tag_jump *jump = xmalloc(sizeof(*jump));
  jump->buffer = editor->window->buffer;
  jump->cursor = window_cursor(editor->window);
  jump->tag = tag;

  if (editor->window->tag) {
    struct tag_jump *j, *tj;
    TAILQ_FOREACH_REVERSE_SAFE(j, &editor->window->tag_stack, tag_list, pointers, tj) {
      if (j == editor->window->tag) {
        break;
      }
      TAILQ_REMOVE(&editor->window->tag_stack, j, pointers);
      free(j);
    }
  }
  TAILQ_INSERT_TAIL(&editor->window->tag_stack, jump, pointers);
  editor->window->tag = jump;

  editor_open(editor, tag->path);
  editor_jump_to_match(editor, tag->cmd + 1, 0, SEARCH_FORWARDS);
}

void editor_tag_stack_prev(struct editor *editor) {
  if (TAILQ_EMPTY(&editor->window->tag_stack)) {
    editor_status_err(editor, "tag stack empty");
  } else if (!editor->window->tag) {
    editor_status_err(editor, "at bottom of tag stack");
  } else {
    buf_clear(editor->status);
    editor->status_error = false;
    window_set_buffer(editor->window, editor->window->tag->buffer);
    window_set_cursor(editor->window, editor->window->tag->cursor);
    editor->window->tag = TAILQ_PREV(editor->window->tag, tag_list, pointers);
  }
}

void editor_tag_stack_next(struct editor *editor) {
  if (TAILQ_EMPTY(&editor->window->tag_stack)) {
    editor_status_err(editor, "tag stack empty");
    return;
  }

  struct tag_jump *next;
  if (!editor->window->tag) {
    next = TAILQ_FIRST(&editor->window->tag_stack);
  } else {
    next = TAILQ_NEXT(editor->window->tag, pointers);
  }
  if (!next) {
    editor_status_err(editor, "at top of tag stack");
    return;
  }

  editor_open(editor, next->tag->path);
  editor_jump_to_match(editor, next->tag->cmd + 1, 0, SEARCH_FORWARDS);
  editor->window->tag = next;
}

EDITOR_COMMAND(tag, tag) {
  if (!arg) {
    editor_tag_stack_next(editor);
  } else {
    editor_jump_to_tag(editor, arg);
  }
}
