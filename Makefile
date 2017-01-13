ifneq (,$(findstring clang,$(realpath $(shell which $(CC)))))
	WARNING_FLAGS = -Weverything -Wno-padded
else
	WARNING_FLAGS = -Wall -Wextra
endif

ifeq ($(COVERAGE),1)
	COVERAGE_FLAG = -coverage
else
	COVERAGE_FLAG =
endif

CFLAGS = $(WARNING_FLAGS) $(COVERAGE_FLAG) -Werror -D_GNU_SOURCE -std=c99 -g $(EXTRA_FLAGS)
LDLIBS = -ltermbox
OUTPUT_OPTION = -MMD -MP -o $@

PROG = badavi
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
HDRS = $(wildcard *.h)

$(PROG): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

-include $(DEPS)

tags: $(SRCS) $(HDRS)
	ctags $^


TEST_PROG = $(PROG)_test
TEST_SRCS = $(wildcard tests/*.c)
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_CFLAGS = -w -std=c99 -I. -Itests/clar -g -D_GNU_SOURCE \
	-DCLAR_FIXTURE_PATH=\"$(abspath tests/testdata)\" $(EXTRA_FLAGS)

tests/%.o: tests/%.c
	$(CC) $(TEST_CFLAGS) -c -o $@ $^

tests/clar/clar.o: tests/clar/clar.c tests/clar/clar.suite
	$(CC) $(TEST_CFLAGS) -c -o $@ $^

tests/clar/clar.suite: $(TEST_SRCS)
	python tests/clar/generate.py tests
	mv tests/clar.suite tests/clar

$(TEST_PROG): $(OBJS) $(TEST_OBJS) tests/clar/clar.o
	$(CC) -o $@ $(LDFLAGS) $(COVERAGE_FLAG) $(filter-out main.o,$^) $(LDLIBS)

.PHONY: test
test: $(TEST_PROG)
	./$(TEST_PROG)

.PHONY: coverage
coverage:
	rm -rf $@
	$(MAKE) -B COVERAGE=1 $(TEST_PROG)
	lcov -q -c -i -d . -o coverage.base
	./$(TEST_PROG)
	lcov -q -c -d . -o coverage.run
	lcov -q -d . -a coverage.base -a coverage.run -o coverage.total
	genhtml -q --no-branch-coverage -o $@ coverage.total
	$(MAKE) clean

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(TEST_OBJS) \
	$(SRCS:.c=.gcda) $(SRCS:.c=.gcno) \
	tests/clar/clar.o tests/clar/clar.suite tests/.clarcache \
	coverage.base coverage.run coverage.total $(PROG) $(PROG)_test
