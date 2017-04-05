#include "buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "gap.h"
#include "list.h"
#include "util.h"

static struct buffer *buffer_of(char *path, struct gapbuf *gb) {
  struct buffer *buffer = xmalloc(sizeof(*buffer));

  buffer->text = gb;
  strcpy(buffer->name, path ? path : "");
  buffer->dirty = false;

  buffer->undo_stack = list_create();
  buffer->redo_stack = list_create();

  buffer->marks = list_create();

  return buffer;
}

struct buffer *buffer_create(char *path) {
  return buffer_of(path, gb_create());
}

struct buffer *buffer_open(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  struct gapbuf *gb = gb_load(fp);
  fclose(fp);

  return buffer_of(path, gb);
}

bool buffer_write(struct buffer *buffer) {
  if (!buffer->name[0]) {
    return false;
  }
  return buffer_saveas(buffer, buffer->name);
}

bool buffer_saveas(struct buffer *buffer, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return false;
  }

  if (!buffer->name[0]) {
    strcpy(buffer->name, path);
  }

  gb_save(buffer->text, fp);
  buffer->dirty = false;
  fclose(fp);
  return true;
}

// A single edit action -- either an insert or delete.
struct edit_action {
  enum { EDIT_ACTION_INSERT, EDIT_ACTION_DELETE } type;
  // The position at which the action occurred.
  size_t pos;
  // The text added (for insertions) or removed (for deletions).
  struct buf *buf;
};

static void buffer_update_marks_after_insert(
    struct buffer *buffer, size_t pos, size_t n) {
  struct region *region;
  LIST_FOREACH(buffer->marks, region) {
    assert(region->end - region->start == 1);
    if (pos <= region->start) {
      region->start += n;
      region->end += n;
    }
  }
}

static void buffer_update_marks_after_delete(
    struct buffer *buffer, size_t pos, size_t n) {
  struct region *region;
  LIST_FOREACH(buffer->marks, region) {
    assert(region->end - region->start == 1);
    if (pos <= region->start) {
      size_t diff = min(n, region->start - pos);
      region->start -= diff;
      region->end -= diff;
    }
  }
}

void buffer_do_insert(struct buffer *buffer, struct buf *buf, size_t pos) {
  struct edit_action *action = xmalloc(sizeof(*action));
  action->type = EDIT_ACTION_INSERT;
  action->pos = pos;
  action->buf = buf;
  struct list *group = list_first(buffer->undo_stack);
  if (group) {
    list_prepend(group, action);
  }
  gb_putstring(buffer->text, buf->buf, buf->len, pos);
  buffer->dirty = true;

  buffer_update_marks_after_insert(buffer, pos, buf->len);
}

void buffer_do_delete(struct buffer *buffer, size_t n, size_t pos) {
  struct edit_action *action = xmalloc(sizeof(*action));
  struct buf *buf = buf_create(n + 1);
  gb_getstring(buffer->text, pos, n, buf->buf);
  buf->len = n;
  action->type = EDIT_ACTION_DELETE;
  action->pos = pos;
  action->buf = buf;
  struct list *group = list_first(buffer->undo_stack);
  if (group) {
    list_prepend(group, action);
  }
  gb_del(buffer->text, n, pos + n);
  buffer->dirty = true;

  buffer_update_marks_after_delete(buffer, pos, n);
}

bool buffer_undo(struct buffer* buffer) {
  if (list_empty(buffer->undo_stack)) {
    return false;
  }

  struct list *group = list_pop(buffer->undo_stack);

  struct gapbuf *gb = buffer->text;
  struct edit_action *action;
  LIST_FOREACH(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      buffer_update_marks_after_delete(buffer, action->pos, action->buf->len);
      break;
    case EDIT_ACTION_DELETE:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      buffer_update_marks_after_insert(buffer, action->pos, action->buf->len);
      break;
    }
  }

  list_prepend(buffer->redo_stack, group);
  return true;
}

bool buffer_redo(struct buffer* buffer) {
  if (list_empty(buffer->redo_stack)) {
    return false;
  }

  struct list *group = list_pop(buffer->redo_stack);

  struct gapbuf *gb = buffer->text;
  struct edit_action *action;
  LIST_FOREACH_REVERSE(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      buffer_update_marks_after_insert(buffer, action->pos, action->buf->len);
      break;
    case EDIT_ACTION_DELETE:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      buffer_update_marks_after_delete(buffer, action->pos, action->buf->len);
      break;
    }
  }

  list_prepend(buffer->undo_stack, group);
  return true;
}

static void edit_action_free(void *p) {
  struct edit_action *action = p;
  buf_free(action->buf);
  free(action);
}

static void edit_action_group_free(void *p) {
  struct list *group = p;
  list_free(group, edit_action_free);
}

void buffer_start_action_group(struct buffer *buffer) {
  list_clear(buffer->redo_stack, edit_action_group_free);
  list_prepend(buffer->undo_stack, list_create());
}
