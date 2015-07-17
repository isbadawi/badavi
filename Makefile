ifneq (,$(findstring clang,$(realpath $(shell which $(CC)))))
	WARNING_FLAGS = -Weverything -Wno-padded
else
	WARNING_FLAGS = -Wall -Wextra
endif

CFLAGS = $(WARNING_FLAGS) -Werror -D_GNU_SOURCE -std=c99 -g
LDLIBS = -ltermbox
OUTPUT_OPTION = -MMD -MP -o $@

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
HDRS = $(wildcard *.h)

badavi: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

-include $(DEPS)

tags: $(SRCS) $(HDRS)
	ctags $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS)
