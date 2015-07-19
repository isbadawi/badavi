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


TEST_SRCS = $(wildcard tests/*.c) tests/clar/clar.c
TEST_CFLAGS = -w -std=c99 -g $(LDLIBS) -I. -Itests/clar -g \
	-DCLAR_FIXTURE_PATH=\"$(abspath tests/testdata)\"

.PHONY: test
test: $(TEST_SRCS) $(OBJS)
	python tests/clar/generate.py tests
	mv tests/clar.suite tests/clar
	$(CC) -o test $(TEST_CFLAGS) $(TEST_SRCS) $(filter-out main.o,$(OBJS))
	./test
	rm test tests/clar/clar.suite
