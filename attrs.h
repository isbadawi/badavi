#pragma once

#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_PRINTFLIKE(fmtarg, firstvararg) \
  __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#define ATTR_UNUSED __attribute__((unused))
