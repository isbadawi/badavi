CC = clang
CFLAGS = -Weverything $(WARNING_EXCEPTIONS) -Werror -g
LDLIBS = -ltermbox
OUTPUT_OPTION = -MMD -MP -o $@

WARNING_EXCEPTIONS = \
	-Wno-assign-enum \
	-Wno-conversion \
	-Wno-format-nonliteral \
	-Wno-missing-field-initializers \
	-Wno-missing-noreturn \
	-Wno-missing-prototypes \
	-Wno-missing-variable-declarations \
	-Wno-padded \
	-Wno-shorten-64-to-32 \
	-Wno-sign-conversion \
	-Wno-unused-parameter \

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
