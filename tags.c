#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "buf.h"
#include "editor.h"
#include "list.h"
#include "search.h"
#include "util.h"
#include "window.h"

static char *escape_regex(char *regex) {
  // Worst case, every char is escaped...
  size_t len = strlen(regex);
  char *result = xmalloc(len * 2 + 1);
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
  tags->tags = xmalloc(sizeof(*tags->tags) * nlines);
  tags->len = nlines;

  size_t i = 0;
  while ((len = getline(&line, &n, fp)) != -1) {
    line[len - 1] = '\0';
    if (*line == '!') {
      continue;
    }
    struct tag_t *tag = &tags->tags[i++];
    tag->name = xstrdup(strtok(line, "\t"));
    tag->path = xstrdup(strtok(NULL, "\t"));
    tag->cmd = escape_regex(strtok(NULL, "\""));
    tag->cmd[strlen(tag->cmd) - 2] = '\0';
  }
  free(line);
  fclose(fp);
}

struct tags_t *tags_create(char *file) {
  struct tags_t *tags = xmalloc(sizeof(*tags));
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

void editor_jump_to_tag(struct editor_t *editor, char *name) {
  struct tag_t *tag = tags_find(editor->tags, name);
  if (!tag) {
    editor_status_err(editor, "tag not found: %s", name);
    return;
  }

  struct tag_jump_t *jump = xmalloc(sizeof(*jump));
  jump->buffer = editor->window->buffer;
  jump->cursor = window_cursor(editor->window);
  jump->tag = tag;

  if (editor->window->tag) {
    list_insert_after(editor->window->tag_stack, editor->window->tag, jump);

    // TODO(isbadawi): Hack & memory leak because our list is inconvenient...
    struct tag_jump_t *j;
    struct list_t *list = editor->window->tag_stack;
    LIST_FOREACH(list, j) {
      if (j == jump) {
        list->iter->next = list->tail;
        list->tail->prev = list->iter;
        break;
      }
    }

  } else {
    list_append(editor->window->tag_stack, jump);
  }
  editor->window->tag = jump;

  editor_open(editor, tag->path);
  editor_search(editor, tag->cmd + 1, 0, SEARCH_FORWARDS);
}

void editor_tag_stack_prev(struct editor_t *editor) {
  if (list_empty(editor->window->tag_stack)) {
    editor_status_err(editor, "tag stack empty");
  } else if (!editor->window->tag) {
    editor_status_err(editor, "at bottom of tag stack");
  } else {
    buf_clear(editor->status);
    window_set_buffer(editor->window, editor->window->tag->buffer);
    window_set_cursor(editor->window, editor->window->tag->cursor);
    editor->window->tag =
      list_prev(editor->window->tag_stack, editor->window->tag);
  }
}

void editor_tag_stack_next(struct editor_t *editor) {
  if (list_empty(editor->window->tag_stack)) {
    editor_status_err(editor, "tag stack empty");
    return;
  }

  struct tag_jump_t *next;
  if (!editor->window->tag) {
    next = editor->window->tag_stack->head->next->data;
  } else {
    next = list_next(editor->window->tag_stack, editor->window->tag);
  }
  if (!next) {
    editor_status_err(editor, "at top of tag stack");
    return;
  }

  editor_open(editor, next->tag->path);
  editor_search(editor, next->tag->cmd + 1, 0, SEARCH_FORWARDS);
  editor->window->tag = next;
}
