COMMON_CFLAGS := -g -std=c99 -D_GNU_SOURCE $(EXTRA_CFLAGS)

WARNING_CFLAGS := -Wall -Wextra
ifneq (,$(findstring clang,$(realpath $(shell which $(CC)))))
WARNING_CFLAGS := -Weverything -Wno-padded
endif
WARNING_CFLAGS += -Werror

COVERAGE_CFLAGS := $(if $(COVERAGE),-coverage)
CFLAGS := $(COMMON_CFLAGS) $(WARNING_CFLAGS) $(COVERAGE_CFLAGS)
LDLIBS := -ltermbox
BUILD_DIR := build

PROG := badavi
SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h)
OBJS := $(SRCS:.c=.o)
OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))
DEPS := $(OBJS:.o=.d)

TEST_PROG := $(PROG)_test
TEST_SRCS := $(wildcard tests/*.c)
TEST_OBJS := $(TEST_SRCS:.c=.o)
TEST_OBJS := $(addprefix $(BUILD_DIR)/,$(TEST_OBJS))
TEST_OBJS += $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
TEST_OBJS += $(BUILD_DIR)/tests/clar/clar.o
TEST_CFLAGS := $(COMMON_CFLAGS) -w -I. -Itests/clar -I$(BUILD_DIR)/tests/clar \
	-DCLAR_FIXTURE_PATH=\"$(abspath tests/testdata)\"

.PHONY: $(PROG)
$(PROG): $(BUILD_DIR)/$(PROG)

.PHONY: $(TEST_PROG)
$(TEST_PROG): $(BUILD_DIR)/$(TEST_PROG)

.PHONY: test
test: $(BUILD_DIR)/$(TEST_PROG)
	./$^

.PHONY: coverage
coverage:
	$(MAKE) BUILD_DIR=coverage-build COVERAGE=1 $(TEST_PROG)
	lcov -q -c -i -d coverage-build -o coverage-build/coverage.base
	./coverage-build/$(TEST_PROG)
	lcov -q -c -d coverage-build -o coverage-build/coverage.run
	lcov -q -d . -a coverage-build/coverage.base -a coverage-build/coverage.run -o coverage-build/coverage.total
	genhtml -q --no-branch-coverage -o $@ coverage-build/coverage.total

# mkdir_dep allows you to specify a directory as an order-only dependency
# without having to manually define the rule that creates it.
mkdir_dep = $(eval $(call mkdir_template,$(1))) $(1)

define mkdir_template
ifndef __mkdir_$(1)_defined__
__mkdir_$(1)_defined__ := 1

$(1):
	mkdir -p $$@

endif
endef

# We define the rule for test objects first because in GNU make 3.81, when
# multiple pattern rules match a target, the first one is chosen. This is
# different than 3.82 and later, where the most specific one (i.e. the one with
# the shortest stem) is chosen.
$(BUILD_DIR)/tests/%.o: tests/%.c \
	| $(call mkdir_dep,$(BUILD_DIR)/tests)
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/clar/clar.o: $(BUILD_DIR)/tests/clar/clar.suite \
	| $(call mkdir_dep,$(BUILD_DIR)/tests/clar)

$(BUILD_DIR)/tests/clar/clar.suite: $(TEST_SRCS) \
	| $(call mkdir_dep,$(BUILD_DIR)/tests/clar)
	python tests/clar/generate.py tests
	mv tests/clar.suite $@

$(BUILD_DIR)/%.o: %.c | $(call mkdir_dep,$(BUILD_DIR))
	$(CC) -MMD -MP -o $@ -c $< $(CFLAGS)

-include $(DEPS)

define program_template
$(1): $(2) | $$(call mkdir_dep,$(BUILD_DIR))
	$(CC) -o $$@ $(LDFLAGS) $(COVERAGE_CFLAGS) $$^ $(LDLIBS)
endef
$(eval $(call program_template,$(BUILD_DIR)/$(PROG),$(OBJS)))
$(eval $(call program_template,$(BUILD_DIR)/$(TEST_PROG),$(TEST_OBJS)))

tags: $(SRCS) $(HDRS)
	ctags $^
