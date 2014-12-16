#ifndef _util_h_included
#define _util_h_included

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

/*
  Reads an entire file into a string.
  Returns NULL if an error occurred.
*/
char *read_file(char *filename);

/*
  Replaces all newlines in a string with null terminators, and returns the number
  of lines. Afterwards, the lines can iterated over like this:

  int nlines = split_lines(buf);
  char *line = buf;
  for (int i = 0; i < nlines; ++i) {
    // use line
    line = line + strlen(line) + 1;
  }
*/
int split_lines(char *buf);

#endif
