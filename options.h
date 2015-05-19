#ifndef _options_h_included
#define _options_h_included

int option_exists(const char *name);
int option_get_int(const char *name);
void option_set_int(const char *name, int value);

#endif
