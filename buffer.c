#include "buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "gap.h"
#include "list.h"
#include "util.h"

static struct buffer_t *buffer_of(char *path, struct gapbuf_t *gb) {
  struct buffer_t *buffer = xmalloc(sizeof(*buffer));

  buffer->text = gb;
  strcpy(buffer->name, path ? path : "");
  buffer->dirty = false;

  buffer->undo_stack = list_create();
  buffer->redo_stack = list_create();

  buffer->marks = list_create();

  return buffer;
}

struct buffer_t *buffer_create(char *path) {
  return buffer_of(path, gb_create());
}

struct buffer_t *buffer_open(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  struct gapbuf_t *gb = gb_load(fp);
  fclose(fp);

  return buffer_of(path, gb);
}

bool buffer_write(struct buffer_t *buffer) {
  if (!buffer->name[0]) {
    return false;
  }
  return buffer_saveas(buffer, buffer->name);
}

bool buffer_saveas(struct buffer_t *buffer, char *path) {
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
struct edit_action_t {
  enum { EDIT_ACTION_INSERT, EDIT_ACTION_DELETE } type;
  // The position at which the action occurred.
  size_t pos;
  // The text added (for insertions) or removed (for deletions).
  struct buf_t *buf;
};

static void buffer_update_marks_after_insert(
    struct buffer_t *buffer, size_t pos, size_t n) {
  struct region_t *region;
  LIST_FOREACH(buffer->marks, region) {
    assert(region->end - region->start == 1);
    if (pos <= region->start) {
      region->start += n;
      region->end += n;
    }
  }
}

static void buffer_update_marks_after_delete(
    struct buffer_t *buffer, size_t pos, size_t n) {
  struct region_t *region;
  LIST_FOREACH(buffer->marks, region) {
    assert(region->end - region->start == 1);
    if (pos <= region->start) {
      size_t diff = min(n, region->start - pos);
      region->start -= diff;
      region->end -= diff;
    }
  }
}

void buffer_do_insert(struct buffer_t *buffer, struct buf_t *buf, size_t pos) {
  struct edit_action_t *action = xmalloc(sizeof(*action));
  action->type = EDIT_ACTION_INSERT;
  action->pos = pos;
  action->buf = buf;
  struct list_t *group = list_peek(buffer->undo_stack);
  list_prepend(group, action);
  gb_putstring(buffer->text, buf->buf, buf->len, pos);
  buffer->dirty = true;

  buffer_update_marks_after_insert(buffer, pos, buf->len);
}

void buffer_do_delete(struct buffer_t *buffer, size_t n, size_t pos) {
  struct edit_action_t *action = xmalloc(sizeof(*action));
  struct buf_t *buf = buf_create(n + 1);
  gb_getstring(buffer->text, pos, n, buf->buf);
  buf->len = n;
  action->type = EDIT_ACTION_DELETE;
  action->pos = pos;
  action->buf = buf;
  struct list_t *group = list_peek(buffer->undo_stack);
  list_prepend(group, action);
  gb_del(buffer->text, n, pos + n);
  buffer->dirty = true;

  buffer_update_marks_after_delete(buffer, pos, n);
}

bool buffer_undo(struct buffer_t* buffer) {
  if (list_empty(buffer->undo_stack)) {
    return false;
  }

  struct list_t *group = list_pop(buffer->undo_stack);

  struct gapbuf_t *gb = buffer->text;
  struct edit_action_t *action;
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

bool buffer_redo(struct buffer_t* buffer) {
  if (list_empty(buffer->redo_stack)) {
    return false;
  }

  struct list_t *group = list_pop(buffer->redo_stack);

  struct gapbuf_t *gb = buffer->text;
  struct edit_action_t *action;
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

void buffer_start_action_group(struct buffer_t *buffer) {
  struct list_t *group;
  LIST_FOREACH(buffer->redo_stack, group) {
    struct edit_action_t *action;
    LIST_FOREACH(group, action) {
      buf_free(action->buf);
      free(action);
    }
    list_free(group);
  }
  list_clear(buffer->redo_stack);

  list_prepend(buffer->undo_stack, list_create());
}
