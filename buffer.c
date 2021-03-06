#include "buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "buf.h"
#include "gap.h"
#include "util.h"

static struct buffer *buffer_of(char *path, struct gapbuf *gb, bool dir) {
  struct buffer *buffer = xmalloc(sizeof(*buffer));

  buffer->directory = dir;
  buffer->text = gb;
  buffer->path = path ? xstrdup(path) : NULL;
  memset(&buffer->opt, 0, sizeof(buffer->opt));

  TAILQ_INIT(&buffer->undo_stack);
  TAILQ_INIT(&buffer->redo_stack);

  TAILQ_INIT(&buffer->marks);

  return buffer;
}

struct buffer *buffer_create(char *path) {
  return buffer_of(path, gb_create(), false);
}

static void action_list_clear(struct action_group_list *list) {
  struct edit_action_group *group, *tg;
  TAILQ_FOREACH_SAFE(group, list, pointers, tg) {
    struct edit_action *action, *ta;
    TAILQ_FOREACH_SAFE(action, &group->actions, pointers, ta) {
      buf_free(action->buf);
      free(action);
    }
    TAILQ_REMOVE(list, group, pointers);
    free(group);
  }
}

void buffer_free(struct buffer *buffer) {
  free(buffer->path);
  gb_free(buffer->text);
  action_list_clear(&buffer->undo_stack);
  action_list_clear(&buffer->redo_stack);
  buffer_free_options(buffer);
  free(buffer);
}

static struct buf *listdir(char *path) {
  struct dirent **namelist;
  int n;

  n = scandir(path, &namelist, NULL, alphasort);
  if (n < 0) {
    return NULL;
  }

  struct buf *buf = buf_create(100);
  for (int i = 0; i < n; ++i) {
    struct dirent *entry = namelist[i];
    if (!strcmp(entry->d_name, ".") ||
        !strcmp(entry->d_name, "..")) {
      free(entry);
      continue;
    }

    buf_append(buf, entry->d_name);
    if (entry->d_type == DT_DIR) {
      buf_append(buf, "/");
    }
    buf_append(buf, "\n");
    free(entry);
  }

  if (n == 2) {
    buf_append(buf, "\n");
  }

  free(namelist);
  return buf;
}

struct buffer *buffer_open(char *path) {
  struct gapbuf *gb;
  bool directory = false;
  struct stat info;

  stat(path, &info);
  directory = S_ISDIR(info.st_mode);

  if (directory) {
    struct buf *listing = listdir(path);
    if (!listing) {
      return NULL;
    }
    gb = gb_fromstring(listing);
    buf_free(listing);
  } else {
    gb = gb_fromfile(path);
    if (!gb) {
      return NULL;
    }
  }

  return buffer_of(path, gb, directory);
}

bool buffer_write(struct buffer *buffer) {
  if (!buffer->path) {
    return false;
  }
  return buffer_saveas(buffer, buffer->path);
}

bool buffer_saveas(struct buffer *buffer, char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return false;
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
  struct edit_action_group *group = TAILQ_FIRST(&buffer->undo_stack);
  if (group) {
    struct edit_action *action = xmalloc(sizeof(*action));
    action->type = EDIT_ACTION_INSERT;
    action->pos = pos;
    action->buf = buf;
    TAILQ_INSERT_HEAD(&group->actions, action, pointers);
  }
  gb_putstring(buffer->text, buf->buf, buf->len, pos);
  buffer->opt.modified = true;

  buffer_update_marks_after_insert(buffer, pos, buf->len);

  if (!group) {
    buf_free(buf);
  }
}

void buffer_do_delete(struct buffer *buffer, size_t n, size_t pos) {
  struct edit_action_group *group = TAILQ_FIRST(&buffer->undo_stack);
  if (group) {
    struct edit_action *action = xmalloc(sizeof(*action));
    action->type = EDIT_ACTION_DELETE;
    action->pos = pos;
    action->buf = gb_getstring(buffer->text, pos, n);
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
  action_list_clear(&buffer->redo_stack);

  struct edit_action_group *group = xmalloc(sizeof(*group));
  TAILQ_INIT(&group->actions);

  TAILQ_INSERT_HEAD(&buffer->undo_stack, group, pointers);
}
