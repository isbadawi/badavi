CC = clang
CFLAGS = -Weverything $(WARNING_EXCEPTIONS) -Werror -g
LDLIBS = -ltermbox
OUTPUT_OPTION = -MMD -MP -o $@

WARNING_EXCEPTIONS = \
	-Wno-padded \

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
