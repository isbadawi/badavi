#pragma once

#if __has_attribute(fallthrough)
  #define ATTR_FALLTHROUGH __attribute__((fallthrough))
#else
  #define ATTR_FALLTHROUGH
#endif

#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_PRINTFLIKE(fmtarg, firstvararg) \
  __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#define ATTR_UNUSED __attribute__((unused))
