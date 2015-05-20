#ifndef _options_h_included
#define _options_h_included

#include <stdbool.h>

bool option_exists(const char *name);

bool option_is_bool(const char *name);
bool option_get_bool(const char *name);
void option_set_bool(const char *name, bool value);

bool option_is_int(const char *name);
int option_get_int(const char *name);
void option_set_int(const char *name, int value);

#endif
