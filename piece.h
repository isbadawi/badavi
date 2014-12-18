#ifndef _piece_table_h_included
#define _piece_table_h_included

#include <stdio.h>

// piece.h implements a piece table data structure, as described in Crowley's
// "Data Structures for Text Sequences"
// https://www.cs.unm.edu/~crowley/papers/sds.pdf
//
// The gist: A piece table stores a pointer to the file being edited, a pointer
// to an "add" file, and a doubly linked list of pieces. Each piece represents
// a span of text in one of the files, e.g. a piece might be "original file at
// pos 42, length 10".

#define ORIGINAL 0
#define ADD 1

typedef struct {
  int start;
  int length;
} region_t;

typedef struct piece {
  char which;
  region_t region;
  struct piece *prev;
  struct piece *next;
} piece_t;

typedef struct {
  piece_t *head;
  FILE *original_fp;
  FILE *add_fp;
  int add_pos;
  int size;
} piece_table_t;

piece_table_t *piece_table_new(char *filename);
void piece_table_write(piece_table_t* table, char *filename);
void piece_table_insert(piece_table_t *table, int pos, char c);

void piece_table_delete(piece_table_t *table, int pos);
// Read the text in the given region into buf.
//
// The following preconditions must hold:
// * buf must have at least region.length bytes available.
// * region.start + region.length < table->size
void piece_table_get(piece_table_t *table, region_t region, char* buf);

// Exposed for debugging purposes
void piece_table_dump(piece_table_t *table, FILE* fp);

int piece_table_index_of(piece_table_t *table, char c, int start);
int piece_table_last_index_of(piece_table_t *table, char c, int start);

#endif
