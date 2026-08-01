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
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
        -Wconversion -g -isystem $(ROOT)/vendor -isystem $(ROOT)/.. \
        -DFMT_HEADER_ONLY -MD -MP $(EXTRA_FLAGS)

EXE := $(OUT)/$(NAME).exe

.PHONY: all run clean

all: run

run: $(EXE)
	@$(EXE)

$(EXE): $(SRCS)
	$(CXX) $(FLAGS) $(SRCS) $(LDFLAGS) -o $@

clean:
	rm -rf $(EXE) $(EXE).d $(EXE).dSYM

-include $(EXE).d
