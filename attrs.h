#pragma once

#define ATTR_PRINTFLIKE(fmtarg, firstvararg) \
  __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#define ATTR_UNUSED __attribute__((unused))
