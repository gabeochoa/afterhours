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

# -MD, not -MMD: vendor headers arrive via -isystem, and -MMD omits system
# headers by definition, so dependency tracking would silently cover nothing.
# $(ROOT)/vendor is magic_enum/fmt; $(ROOT)/.. is the directory holding this
# checkout, so <afterhours/...> resolves (what the old -I../../../../../vendor
# was doing in about half the leaves).
EXE := $(OUT)/$(NAME).exe

# -MF is not optional here. Left to itself, -MD names the dep file after the
# output with its extension replaced, so `-o foo.exe` writes foo.d -- not
# foo.exe.d, which is what the -include below looks for. The result compiles
# and looks healthy while tracking nothing: editing a header rebuilds nothing
# and you test a stale binary.
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
        -Wconversion -g -isystem $(ROOT)/vendor -isystem $(ROOT)/.. \
        -DFMT_HEADER_ONLY -MD -MP -MF $(EXE).d $(EXTRA_FLAGS)

.PHONY: all build run clean

all: run

# Compile without running, for CI and for sweeping the catalog.
build: $(EXE)

run: $(EXE)
	@$(EXE)

$(EXE): $(SRCS)
	$(CXX) $(FLAGS) $(SRCS) $(LDFLAGS) -o $@

clean:
	rm -rf $(EXE) $(EXE).d $(EXE).dSYM

-include $(EXE).d
