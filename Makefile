BUILD_DIR := build

CLAR_DIR := vendor/clar

TERMBOX_DIR := vendor/termbox
TERMBOX_INSTALL_DIR := $(BUILD_DIR)/termbox
TERMBOX_HEADER := $(TERMBOX_INSTALL_DIR)/include/termbox.h
TERMBOX_LIBRARY := $(TERMBOX_INSTALL_DIR)/lib/libtermbox.a
TERMBOX := $(TERMBOX_HEADER) $(TERMBOX_LIBRARY)

COMMON_CFLAGS := -g -std=c99 -D_GNU_SOURCE -isystem $(dir $(TERMBOX_HEADER))

WARNING_CFLAGS := -Wall -Wextra
ifneq (,$(findstring clang,$(realpath $(shell which $(CC)))))
WARNING_CFLAGS := -Weverything -Wno-padded
endif
WARNING_CFLAGS += -Werror

COVERAGE_CFLAGS := $(if $(COVERAGE),-coverage)
CFLAGS := $(COMMON_CFLAGS) $(WARNING_CFLAGS) $(COVERAGE_CFLAGS)

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
TEST_OBJS += $(BUILD_DIR)/tests/clar.o
TEST_CFLAGS := $(COMMON_CFLAGS) -w -I. -isystem $(CLAR_DIR) -I$(BUILD_DIR)/tests \
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

# Enable second expansion to access automatic variables in prerequisite lists.
# In particular, we write $$(@D)/. to refer to the directory of the target.
# Note the trailing dot -- make 3.81 seems to ignore trailing slashes.
.SECONDEXPANSION:

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

$(TERMBOX_INSTALL_DIR): | $$(@D)/.
	(cd $(TERMBOX_DIR) && \
	  ./waf configure --prefix=$(abspath $@) && \
	  ./waf && \
	  ./waf install --targets=termbox_static)

$(TERMBOX): $(TERMBOX_INSTALL_DIR)

# We define the rule for test objects first because in GNU make 3.81, when
# multiple pattern rules match a target, the first one is chosen. This is
# different than 3.82 and later, where the most specific one (i.e. the one with
# the shortest stem) is chosen.
$(BUILD_DIR)/tests/%.o: tests/%.c $(TERMBOX_HEADER) | $$(@D)/.
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/clar.o: \
	$(CLAR_DIR)/clar.c $(BUILD_DIR)/tests/clar.suite | $$(@D)/.
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/clar.suite: $(TEST_SRCS) | $$(@D)/.
	$(CLAR_DIR)/generate.py tests
	mv tests/clar.suite $@

$(BUILD_DIR)/%.o: %.c $(TERMBOX_HEADER) | $$(@D)/.
	$(CC) -MMD -MP -o $@ -c $< $(CFLAGS)

-include $(DEPS)

define program_template
$(1): $(2) $(TERMBOX_LIBRARY) | $$$$(@D)/.
	$(CC) -o $$@ $(COVERAGE_CFLAGS) $$^
endef
$(eval $(call program_template,$(BUILD_DIR)/$(PROG),$(OBJS)))
$(eval $(call program_template,$(BUILD_DIR)/$(TEST_PROG),$(TEST_OBJS)))

tags: $(SRCS) $(HDRS)
	ctags $^
