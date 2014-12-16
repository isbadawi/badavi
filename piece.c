#include "piece.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

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
  piece->start = start;
  piece->length = length;
  piece->prev = piece->next = NULL;
  return piece;
}

piece_table_t *piece_table_new(char *filename) {
  piece_table_t *table = malloc(sizeof(piece_table_t));
  if (!table) {
    return NULL;
  }

  table->original_fp = fopen(filename, "r");
  table->size = file_size(table->original_fp);
  table->add_fp = tmpfile();
  table->add_pos = 0;
  table->head = piece_new(ORIGINAL, 0, table->size);

  if (!table->original_fp || !table->add_fp || !table->head) {
    return NULL;
  }
  return table;
}

static piece_t *piece_find(piece_table_t *table, int pos, int *offset) {
  *offset = 0;
  for (piece_t *piece = table->head; piece != NULL; piece = piece->next) {
    if (*offset + piece->length > pos) {
      *offset = pos - *offset;
      return piece;
    }
    *offset += piece->length;
  }
  return NULL;
}

static void link(piece_t* prev, piece_t* next) {
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
      piece->which, piece->start + offset, piece->length - offset + 1);

  fseek(table->add_fp, table->add_pos++, SEEK_SET);
  fputc(c, table->add_fp);

  link(right_piece, piece->next);
  link(piece, new_piece);
  link(new_piece, right_piece);

  table->size++;
}

void piece_table_delete(piece_table_t *table, int pos) {
  int offset;
  piece_t *piece = piece_find(table, pos, &offset);
  if (!piece) {
    return;
  }

  piece_t* new_piece = piece_new(
      piece->which, piece->start + offset + 1, piece->length - offset - 1);
  piece->length = offset;

  link(new_piece, piece->next);
  link(piece, new_piece);

  table->size--;
}

char piece_table_get(piece_table_t *table, int pos) {
  int offset;
  piece_t *piece = piece_find(table, pos, &offset);
  if (!piece) {
    return -1;
  }
  FILE *fp = piece->which == ADD ? table->add_fp : table->original_fp;
  fseek(fp, piece->start + offset, SEEK_SET);
  return fgetc(fp);
}

void piece_table_dump(piece_table_t* table, FILE *fp) {
  fprintf(fp, "File     Start    Length   (Offset)\n");
  int offset = 0;
  for (piece_t* piece = table->head; piece != NULL; piece = piece->next) {
    fprintf(fp, "%s", piece->which == ADD ? "Add      " : "Original ");
    fprintf(fp, "%-8d %-8d %-8d\n", piece->start, piece->length, offset);
    offset += piece->length;
  }
  fflush(fp);
}

void piece_table_write(piece_table_t* table, char *filename) {
  FILE *fp = fopen(filename, "w");
  for (int i = 0; i < table->size; ++i) {
    fputc(piece_table_get(table, i), fp);
  }
  fclose(fp);
}
