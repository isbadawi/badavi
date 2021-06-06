BUILD_DIR := build

CLAR_DIR := vendor/clar

TERMBOX_DIR := vendor/termbox
TERMBOX_INSTALL_DIR := $(BUILD_DIR)/termbox
TERMBOX_BUILD_DIR := $(TERMBOX_INSTALL_DIR)/build
TERMBOX_HEADER := $(TERMBOX_INSTALL_DIR)/include/termbox.h
TERMBOX_LIBRARY := $(TERMBOX_INSTALL_DIR)/lib/libtermbox.a
TERMBOX := $(TERMBOX_HEADER) $(TERMBOX_LIBRARY)

LIBCLIPBOARD_DIR := vendor/libclipboard
LIBCLIPBOARD_INSTALL_DIR := $(BUILD_DIR)/libclipboard
LIBCLIPBOARD_BUILD_DIR := $(LIBCLIPBOARD_INSTALL_DIR)/build
LIBCLIPBOARD_HEADER := $(LIBCLIPBOARD_INSTALL_DIR)/include/libclipboard.h
LIBCLIPBOARD_LIBRARY := $(LIBCLIPBOARD_INSTALL_DIR)/lib/libclipboard.a
LIBCLIPBOARD := $(LIBCLIPBOARD_HEADER) $(LIBCLIPBOARD_LIBRARY)

PCRE2_DIR := vendor/pcre2
PCRE2_INSTALL_DIR := $(BUILD_DIR)/pcre2
PCRE2_BUILD_DIR := $(PCRE2_INSTALL_DIR)/build
PCRE2_HEADER := $(PCRE2_INSTALL_DIR)/include/pcre2.h
PCRE2_LIBRARY := $(PCRE2_INSTALL_DIR)/lib/libpcre2-8.a
PCRE2 := $(PCRE2_HEADER) $(PCRE2_LIBRARY)

OS := $(shell uname)
ifeq ($(OS),Darwin)
LIBCLIPBOARD_LDFLAGS := -framework Cocoa
else ifeq ($(OS),Linux)
LIBCLIPBOARD_LDFLAGS := -lxcb -lpthread
endif
LDFLAGS := $(LIBCLIPBOARD_LDFLAGS)

THIRD_PARTY_HEADERS := $(TERMBOX_HEADER) $(LIBCLIPBOARD_HEADER) $(PCRE2_HEADER)
THIRD_PARTY_LIBRARIES := $(TERMBOX_LIBRARY) $(LIBCLIPBOARD_LIBRARY) $(PCRE2_LIBRARY)

WARNING_CFLAGS := -Wall -Wextra -Werror

COVERAGE_CFLAGS := $(if $(COVERAGE),-coverage)
ASAN_CFLAGS := $(if $(ASAN),-fsanitize=address -fsanitize-recover=address)
LDFLAGS += $(COVERAGE_CFLAGS) $(ASAN_CFLAGS)

# Use C11 for anonymous structs.
COMMON_CFLAGS := -g -std=c11 -D_GNU_SOURCE \
	$(addprefix -isystem ,$(dir $(THIRD_PARTY_HEADERS))) \
	$(WARNING_CFLAGS)

CFLAGS := $(COMMON_CFLAGS) $(COVERAGE_CFLAGS) $(ASAN_CFLAGS)

TEST_CFLAGS := $(COMMON_CFLAGS) -Wno-missing-prototypes \
	-I. -isystem $(CLAR_DIR) -I$(BUILD_DIR)/tests \
	-DCLAR_FIXTURE_PATH=\"$(abspath tests/testdata)\"

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
TEST_DEPS := $(TEST_OBJS:.o=.d)
TEST_OBJS += $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
TEST_OBJS += $(BUILD_DIR)/tests/clar.o

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

.PHONY: asan
asan:
	$(MAKE) BUILD_DIR=asan-build ASAN=1 $(PROG)

.PHONY: test-asan
test-asan:
	$(MAKE) BUILD_DIR=asan-build ASAN=1 $(TEST_PROG)
	./asan-build/$(TEST_PROG)

# Enable second expansion to access automatic variables in prerequisite lists.
# In particular, we write $$(@D)/. to refer to the directory of the target.
# Note the trailing dot -- make 3.81 seems to ignore trailing slashes.
.SECONDEXPANSION:

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

define cmake_dep
$$($(1)_INSTALL_DIR): | $$$$(@D)/. $$($(1)_BUILD_DIR)/.
	(cd $$($(1)_BUILD_DIR) && \
	  cmake -DCMAKE_INSTALL_PREFIX=$$(abspath $$@) $(2) $$(abspath $$($(1)_DIR)) && \
	  $(MAKE) && \
	  $(MAKE) install)

$$($(1)): $$($(1)_INSTALL_DIR)
endef

$(eval $(call cmake_dep,TERMBOX,-DBUILD_SHARED_LIBS=OFF -DBUILD_DEMOS=OFF))
$(eval $(call cmake_dep,LIBCLIPBOARD))
$(eval $(call cmake_dep,PCRE2,-DPCRE2_BUILD_PCRE2GREP=OFF -DPCRE2_BUILD_TESTS=OFF))

# We define the rule for test objects first because in GNU make 3.81, when
# multiple pattern rules match a target, the first one is chosen. This is
# different than 3.82 and later, where the most specific one (i.e. the one with
# the shortest stem) is chosen.
$(BUILD_DIR)/tests/%.o: tests/%.c $(THIRD_PARTY_HEADERS) | $$(@D)/.
	$(CC) -MMD -MP $(TEST_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/clar.o: \
	$(CLAR_DIR)/clar.c $(BUILD_DIR)/tests/clar.suite | $$(@D)/.
	$(CC) $(TEST_CFLAGS) -w -c -o $@ $<

$(BUILD_DIR)/tests/clar.suite: $(TEST_SRCS) | $$(@D)/.
	$(CLAR_DIR)/generate.py --output=$(@D) tests

$(BUILD_DIR)/%.o: %.c $(THIRD_PARTY_HEADERS) | $$(@D)/.
	$(CC) -MMD -MP -o $@ -c $< $(CFLAGS)

$(BUILD_DIR)/%.pp: %.c $(THIRD_PARTY_HEADERS) | $$(@D)/.
	$(CC) -E -o $@ -c $< $(CFLAGS)

-include $(DEPS)
-include $(TEST_DEPS)

$(BUILD_DIR)/$(PROG): $(OBJS) $(THIRD_PARTY_LIBRARIES) | $$(@D)/.
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/$(TEST_PROG): $(TEST_OBJS) $(filter-out $(TERMBOX_LIBRARY),$(THIRD_PARTY_LIBRARIES)) | $$(@D)/.
	$(CC) -o $@ $^ $(LDFLAGS)

tags: $(SRCS) $(HDRS)
	ctags $^
