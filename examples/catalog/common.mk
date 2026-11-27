# Shared build for catalog examples. Include LAST -- this file defines the
# targets, so a leaf sets its variables first and then includes:
#
#   NAME = basic
#   include ../../common.mk
#
# Knobs a leaf can set before including:
#   NAME        binary name, minus .exe   (default: the directory name)
#   SRCS        sources                   (default: main.cpp)
#   EXTRA_FLAGS appended to FLAGS         (e.g. -O3, -DAFTER_HOURS_USE_RAYLIB)
#   LDFLAGS     link flags                (e.g. raylib)
#   CXX         compiler                  (default: clang++)
#   TEST_TARGET target `test` runs        (default: run, or e2e when the leaf
#                                        sets .DEFAULT_GOAL := e2e)
#   NO_DEFAULT_TEST=1  leaf defines its own `test:` after the include
#
# Extra targets go after the include; `all` stays the default goal because it
# is the first target defined here.

# Derived from MAKEFILE_LIST so an example that moves category keeps working.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)
OUT  := $(ROOT)/output
$(shell mkdir -p $(OUT))

CXX  ?= clang++
NAME ?= $(notdir $(CURDIR))
SRCS ?= main.cpp

EXE := $(OUT)/$(NAME).exe

BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null)
ifeq ($(strip $(BREW_PREFIX)),)
    BREW_PREFIX := /opt/homebrew
endif

# Respect a RAYLIB_PREFIX from the environment/CLI or the leaf; otherwise ask
# brew, otherwise the classic Apple-Silicon Cellar path. Works the same with
# /opt/homebrew and a per-user ~/homebrew.
RAYLIB_PREFIX ?= $(shell brew --prefix raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_PREFIX)),)
    RAYLIB_PREFIX := /opt/homebrew/Cellar/raylib/5.5
endif

# Pin <afterhours/...> to THIS checkout regardless of the directory's name.
# A bare `-isystem $(ROOT)/..` resolves <afterhours/...> to the *parent* of
# the checkout dir, which only works when the checkout is literally named
# `afterhours`; in any other clone it silently picks up a sibling checkout
# (or fails outright). Same pattern as examples/Makefile: a private include
# dir holding a symlink named exactly `afterhours` -> this checkout root.
AFH_INCLUDE := $(ROOT)/examples/catalog/.afh-include
$(shell mkdir -p $(AFH_INCLUDE) && ln -sfn $(ROOT) $(AFH_INCLUDE)/afterhours)

# -MD, not -MMD: vendor headers arrive via -isystem, and -MMD omits system
# headers by definition, so dependency tracking would silently cover nothing.
# $(ROOT)/vendor is magic_enum/fmt.
# -MF is not optional here. Left to itself, -MD names the dep file after the
# output with its extension replaced, so `-o foo.exe` writes foo.d -- not
# foo.exe.d, which is what the -include below looks for. The result compiles
# and looks healthy while tracking nothing: editing a header rebuilds nothing
# and you test a stale binary.
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
        -Wconversion -g -isystem $(ROOT)/vendor -isystem $(BREW_PREFIX)/include \
        -isystem $(AFH_INCLUDE) -DFMT_HEADER_ONLY -MD -MP -MF $(EXE).d $(EXTRA_FLAGS)

.PHONY: all build run clean test

all: run

# Compile without running, for CI and for sweeping the catalog.
build: $(EXE)

run: $(EXE)
	@$(EXE)

$(EXE): $(SRCS)
	$(CXX) $(FLAGS) $(SRCS) $(LDFLAGS) -o $@

clean:
	rm -rf $(EXE) $(EXE).d $(EXE).dSYM

# Standard test entry point: `make -C <example> test` builds and runs the
# example as a self-verifying test. The prerequisite is resolved after the
# whole leaf makefile has been read (secondary expansion), so a leaf that
# sets `.DEFAULT_GOAL := e2e` after the include tests through its `e2e`
# target, and a leaf may point TEST_TARGET at a custom bounded target.
# A leaf with a fully custom test recipe sets NO_DEFAULT_TEST=1 before the
# include and defines its own `test:` afterwards.
TEST_TARGET ?= run
TEST_PREREQ = $(if $(filter e2e,$(.DEFAULT_GOAL)),e2e,$(TEST_TARGET))
ifndef NO_DEFAULT_TEST
.SECONDEXPANSION:
test: $$(TEST_PREREQ)
endif

-include $(EXE).d
