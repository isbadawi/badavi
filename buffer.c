#include "buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "gap.h"
#include "util.h"

static struct buffer *buffer_of(char *path, struct gapbuf *gb) {
  struct buffer *buffer = xmalloc(sizeof(*buffer));

  buffer->text = gb;
  strcpy(buffer->name, path ? path : "");
  buffer->opt.modified = false;

  TAILQ_INIT(&buffer->undo_stack);
  TAILQ_INIT(&buffer->redo_stack);

  TAILQ_INIT(&buffer->marks);

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
  buffer->opt.modified = false;
  fclose(fp);
  return true;
}

static void buffer_update_marks_after_insert(
    struct buffer *buffer, size_t pos, size_t n) {
  struct mark *mark;
  TAILQ_FOREACH(mark, &buffer->marks, pointers) {
    assert(mark->region.end - mark->region.start == 1);
    if (pos <= mark->region.start) {
      mark->region.start += n;
      mark->region.end += n;
    }
  }
}

static void buffer_update_marks_after_delete(
    struct buffer *buffer, size_t pos, size_t n) {
  struct mark *mark;
  TAILQ_FOREACH(mark, &buffer->marks, pointers) {
    assert(mark->region.end - mark->region.start == 1);
    if (pos <= mark->region.start) {
      size_t diff = min(n, mark->region.start - pos);
      mark->region.start -= diff;
      mark->region.end -= diff;
    }
  }
}

void buffer_do_insert(struct buffer *buffer, struct buf *buf, size_t pos) {
  struct edit_action *action = xmalloc(sizeof(*action));
  action->type = EDIT_ACTION_INSERT;
  action->pos = pos;
  action->buf = buf;
  struct edit_action_group *group = TAILQ_FIRST(&buffer->undo_stack);
  if (group) {
    TAILQ_INSERT_HEAD(&group->actions, action, pointers);
  }
  gb_putstring(buffer->text, buf->buf, buf->len, pos);
  buffer->opt.modified = true;

  buffer_update_marks_after_insert(buffer, pos, buf->len);
}

void buffer_do_delete(struct buffer *buffer, size_t n, size_t pos) {
  struct edit_action *action = xmalloc(sizeof(*action));
  action->type = EDIT_ACTION_DELETE;
  action->pos = pos;
  action->buf = gb_getstring(buffer->text, pos, n);
  struct edit_action_group *group = TAILQ_FIRST(&buffer->undo_stack);
  if (group) {
    TAILQ_INSERT_HEAD(&group->actions, action, pointers);
  }
  gb_del(buffer->text, n, pos + n);
  buffer->opt.modified = true;

  buffer_update_marks_after_delete(buffer, pos, n);
}

bool buffer_undo(struct buffer* buffer, size_t *cursor_pos) {
  struct edit_action_group *group = TAILQ_FIRST(&buffer->undo_stack);
  if (!group) {
    return false;
  }

  TAILQ_REMOVE(&buffer->undo_stack, group, pointers);

  struct gapbuf *gb = buffer->text;
  struct edit_action *action;
  TAILQ_FOREACH(action, &group->actions, pointers) {
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

  TAILQ_INSERT_HEAD(&buffer->redo_stack, group, pointers);
  *cursor_pos = TAILQ_LAST(&group->actions, action_list)->pos;
  return true;
}

bool buffer_redo(struct buffer* buffer, size_t *cursor_pos) {
  struct edit_action_group *group = TAILQ_FIRST(&buffer->redo_stack);
  if (!group) {
    return false;
  }

  TAILQ_REMOVE(&buffer->redo_stack, group, pointers);

  struct gapbuf *gb = buffer->text;
  struct edit_action *action;
  TAILQ_FOREACH_REVERSE(action, &group->actions, action_list, pointers) {
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

  TAILQ_INSERT_HEAD(&buffer->undo_stack, group, pointers);
  *cursor_pos = TAILQ_LAST(&group->actions, action_list)->pos;
  return true;
}

void buffer_start_action_group(struct buffer *buffer) {
  struct edit_action_group *group, *tg;
  TAILQ_FOREACH_SAFE(group, &buffer->redo_stack, pointers, tg) {
    struct edit_action *action, *ta;
    TAILQ_FOREACH_SAFE(action, &group->actions, pointers, ta) {
      buf_free(action->buf);
      free(action);
    }
    TAILQ_REMOVE(&buffer->redo_stack, group, pointers);
    free(group);
  }

  group = xmalloc(sizeof(*group));
  TAILQ_INIT(&group->actions);

  TAILQ_INSERT_HEAD(&buffer->undo_stack, group, pointers);
}
