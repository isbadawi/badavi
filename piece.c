#include "piece.h"

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

static off_t file_size(FILE *fp) {
  struct stat info;
  fstat(fileno(fp), &info);
  return info.st_size;
}

piece_t *piece_new(char which, int start, int length) {
  piece_t *piece = malloc(sizeof(piece_t));
  if (!piece) {
    return NULL;
  }
  piece->which = which;
  piece->region.start = start;
  piece->region.length = length;
  piece->prev = piece->next = NULL;
  return piece;
}

piece_table_t *piece_table_new(char *filename) {
  piece_table_t *table = malloc(sizeof(piece_table_t));
  if (!table) {
    return NULL;
  }

  if (access(filename, F_OK) == -1) {
    table->original_fp = NULL;
    table->size = 0;
  } else {
    table->original_fp = fopen(filename, "r");
    table->size = file_size(table->original_fp);
  }
  table->add_fp = tmpfile();
  table->add_pos = 0;
  table->head = piece_new(ORIGINAL, 0, table->size);

  if (!table->add_fp || !table->head) {
    return NULL;
  }
  return table;
}

static piece_t *piece_find(piece_table_t *table, int pos, int *offset) {
  *offset = 0;
  for (piece_t *piece = table->head; piece != NULL; piece = piece->next) {
    int len = piece->region.length;
    if (*offset + len > pos) {
      *offset = pos - *offset;
      return piece;
    }
    *offset += len;
  }
  return NULL;
}

static void piece_link(piece_t* prev, piece_t* next) {
  if (prev) {
    prev->next = next;
  }
  if (next) {
    next->prev = prev;
  }
}

void piece_table_insert(piece_table_t *table, int pos, char c) {
  int offset;
  piece_t *piece = piece_find(table, pos, &offset);
  if (!piece) {
    return;
  }

  piece_t *new_piece = piece_new(ADD, table->add_pos, 1);
  piece_t *right_piece = piece_new(
      piece->which,
      piece->region.start + offset,
      piece->region.length - offset + 1);
  piece->region.length = offset;

  fseek(table->add_fp, table->add_pos++, SEEK_SET);
  fputc(c, table->add_fp);

  piece_link(right_piece, piece->next);
  piece_link(piece, new_piece);
  piece_link(new_piece, right_piece);

  table->size++;
}

void piece_table_delete(piece_table_t *table, int pos) {
  int offset;
  piece_t *piece = piece_find(table, pos, &offset);
  if (!piece) {
    return;
  }

  piece_t* new_piece = piece_new(
      piece->which,
      piece->region.start + offset + 1,
      piece->region.length - offset - 1);
  piece->region.length = offset;

  piece_link(new_piece, piece->next);
  piece_link(piece, new_piece);

  table->size--;
}

static int piece_read(
    piece_table_t *table,
    piece_t *piece,
    region_t region,
    char *buf) {
  int readmax = piece->region.length - region.start;
  int n = region.length;
  n = n < 0 ? readmax : min(readmax, n);

  FILE *fp = piece->which == ADD ? table->add_fp : table->original_fp;
  fseek(fp, piece->region.start + region.start, SEEK_SET);
  return fread(buf, 1, n, fp);
}

void piece_table_get(piece_table_t *table, region_t region, char *buf) {
  int offset;
  piece_t *piece = piece_find(table, region.start, &offset);
  region.start = offset;
  while (region.length != 0) {
    int bytes_read = piece_read(table, piece, region, buf);

    buf += bytes_read;

    piece = piece->next;
    region.start = 0;
    region.length -= bytes_read;
  }
}

void piece_table_dump(piece_table_t* table, FILE *fp) {
  fprintf(fp, "File     Start    Length   (Offset)\n");
  int offset = 0;
  for (piece_t* piece = table->head; piece != NULL; piece = piece->next) {
    fprintf(fp, "%s", piece->which == ADD ? "Add      " : "Original ");
    fprintf(fp, "%-8d %-8d %-8d\n",
        piece->region.start, piece->region.length, offset);
    offset += piece->region.length;
  }
  fflush(fp);
}

void piece_table_write(piece_table_t* table, char *filename) {
  char buf[1024 * 1024];
  FILE *fp = fopen(filename, "w");
  for (piece_t *piece = table->head; piece != NULL; piece = piece->next) {
    region_t region = {0, -1};
    int n = piece_read(table, piece, region, buf);
    fwrite(buf, 1, n, fp);
  }
  fclose(fp);

  // The pieces are now stale -- free them and start fresh...
  piece_t *piece = table->head;
  while (piece != NULL) {
    piece_t *temp = piece;
    piece = piece->next;
    free(temp);
  }
  table->head = piece_new(ORIGINAL, 0, table->size);
  // Replace the original file pointer with a newly opened one.
  // This seems to be required, because reads are cached or something.
  FILE *copy = fdopen(dup(fileno(table->original_fp)), "r");
  fclose(table->original_fp);
  table->original_fp = copy;
}

int piece_table_index_of(piece_table_t *table, char c, int start) {
  region_t region;
  region.start = start;
  region.length = min(1024, table->size - start);
  while (region.length > 0) {
    char buf[1024];
    piece_table_get(table, region, buf);
    for (int i = 0; i < region.length; ++i) {
      if (buf[i] == c) {
        return region.start + i;
      }
    }
    region.start += region.length;
    region.length = min(1024, table->size - region.start);
  }
  return -1;
}

int piece_table_last_index_of(piece_table_t *table, char c, int start) {
  region_t region;
  region.start = max(0, start - 1024);
  region.length = min(1024, start + 1);
  while (region.length > 0) {
    char buf[1024];
    piece_table_get(table, region, buf);
    for (int i = region.length - 1; i >= 0; --i) {
      if (buf[i] == c) {
        return region.start + i;
      }
    }
    region.start -= region.length;
    region.length = min(1024, region.start);
  }
  return -1;
}
