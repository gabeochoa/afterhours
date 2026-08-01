# Makefile for afterhours automated tests
#
# Usage:
#   make        - Build all tests
#   make test   - Build and run all tests
#   make clean  - Remove test binaries

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS_RAYLIB := $(CXXFLAGS) -DAFTER_HOURS_USE_RAYLIB

# Homebrew prefix differs per machine (/opt/homebrew on Apple-Silicon default,
# ~/homebrew for a per-user install). Auto-detect via `brew --prefix raylib`
# and fall back to the classic Cellar path. Override on the CLI if needed:
#   make RAYLIB_PREFIX=/path/to/raylib
RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
ifeq ($(RAYLIB_PREFIX),)
    RAYLIB_PREFIX := /opt/homebrew/Cellar/raylib/5.5
endif
RAYLIB_INCLUDE := $(RAYLIB_PREFIX)/include
RAYLIB_LIB := $(RAYLIB_PREFIX)/lib
# fmt is header-only here; its headers live under the Homebrew include dir.
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifeq ($(BREW_PREFIX),)
    BREW_PREFIX := /opt/homebrew
endif
# Pin <afterhours/...> to THIS checkout regardless of the directory's name.
# A bare `-isystem ../..` resolves <afterhours/...> to the *parent* of the
# checkout dir, which only works when the checkout is literally named
# `afterhours`. In a git worktree (or any differently-named clone) that silently
# picks up a *sibling* `afterhours` checkout, so tests compile against the wrong
# source. Generate a private include dir containing a symlink named exactly
# `afterhours` -> this checkout root, and use it as the sole afterhours root.
AFTERHOURS_ROOT := $(realpath ..)
INCLUDE_ROOT := $(CURDIR)/.afh-include
$(shell mkdir -p $(INCLUDE_ROOT) && ln -sfn $(AFTERHOURS_ROOT) $(INCLUDE_ROOT)/afterhours)

# Emits a .d beside each binary listing the headers it pulled in. Without this,
# editing a header rebuilds nothing and you test a stale binary.
#
# -MD, not -MMD: the afterhours headers arrive via -isystem (see INCLUDE_ROOT
# below), which makes them system headers, and -MMD omits those by definition.
# With -MMD the dep files come out not mentioning a single afterhours header —
# tracking that silently does nothing. The cost is SDK headers in the .d too.
DEPFLAGS := -MD -MP

INCLUDES := -isystem $(INCLUDE_ROOT) -isystem $(AFTERHOURS_ROOT)/vendor -isystem $(BREW_PREFIX)/include
INCLUDES_RAYLIB := -I$(RAYLIB_INCLUDE) $(INCLUDES)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_RAYLIB := $(RAYLIB_LIB)/libraylib.a -framework OpenGL -framework CoreFoundation -framework Cocoa -framework IOKit -framework CoreGraphics -framework CoreVideo -lobjc
else ifeq ($(UNAME_S),Linux)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

# All build products land in the repo-wide output/ dir (see
# examples/catalog/common.mk) instead of littering the source tree.
OUT := $(realpath ..)/output
$(shell mkdir -p $(OUT))

ALL_TESTS := \
	autolayout_test \
	dialog_test \
	dump_ui_test \
	entity_mapping_test \
	entity_pool_test \
	entity_query_test \
	input_injector_mouse_delta_test \
	keymap_test \
	progress_bar_test \
	random_engine_test \
	render_order_test \
	setup_test \
	size_constraints_test \
	slider_test \
	split_test \
	downstream_gaps_test \
	stepper_test \
	styling_test \
	system_tags_test \
	tab_container_test \
	text_wrap_test \
	theme_io_test

RAYLIB_TESTS := \
	dialog_test \
	dump_ui_test \
	progress_bar_test \
	render_order_test \
	setup_test \
	size_constraints_test \
	slider_test \
	split_test \
	downstream_gaps_test \
	stepper_test \
	styling_test \
	tab_container_test \
	text_wrap_test \
	theme_io_test

TEST_BINS := $(addprefix $(OUT)/,$(ALL_TESTS))

# Declared after ALL_TESTS so the bare-name aliases are actually included.
.PHONY: all test clean $(ALL_TESTS)

all: $(TEST_BINS)

# Bare names stay usable: `make split_test` builds $(OUT)/split_test.
$(ALL_TESTS): %: $(OUT)/%

# One rule for every test. The raylib split rides on target-specific variables
# rather than a second copy of the recipe.
RAYLIB_BINS := $(addprefix $(OUT)/,$(RAYLIB_TESTS))

CXXFLAGS_T = $(CXXFLAGS)
INCLUDES_T = $(INCLUDES)
LDFLAGS_T  =

$(RAYLIB_BINS): CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(RAYLIB_BINS): INCLUDES_T = $(INCLUDES_RAYLIB)
$(RAYLIB_BINS): LDFLAGS_T  = $(LDFLAGS_RAYLIB)

# $(filter %.cpp,$^) rather than $^: the generated .d files add every header as
# a prerequisite, and a bare $^ would hand all of them to clang as input files.
$(OUT)/%: %.cpp
	$(CXX) $(CXXFLAGS_T) $(DEPFLAGS) $(INCLUDES_T) $(filter %.cpp,$^) $(LDFLAGS_T) -o $@

# Exceptions. A rule with prerequisites and no recipe adds to the pattern rule
# above instead of replacing it.
$(OUT)/entity_mapping_test: ../examples/entity_mapping_test_helper.cpp
$(OUT)/entity_mapping_test: INCLUDES_T += -I../examples
$(OUT)/random_engine_test:  CXXFLAGS_T += -DAFTER_HOURS_ENABLE_RANDOM

# Runs every test even when one fails, then reports. The old loop dropped each
# exit code, so only the last test's status escaped and a red suite read green.
test: all
	@fail=0; for t in $(ALL_TESTS); do \
		echo "Running $$t"; \
		$(OUT)/$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; fi

clean:
	rm -rf $(TEST_BINS) $(addsuffix .d,$(TEST_BINS)) $(addsuffix .dSYM,$(TEST_BINS))
	rm -f input_system_mouse_delta_backend_smoke.o smoke_raylib.o smoke_sokol.o
	rm -rf $(INCLUDE_ROOT)

-include $(wildcard $(OUT)/*.d)
