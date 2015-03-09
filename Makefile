CC = clang
CFLAGS = -Wall -Werror -pedantic -g
LDLIBS = -ltermbox
OUTPUT_OPTION = -MMD -MP -o $@

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

badavi: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

-include $(DEPS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS)
