# Makefile for afterhours graphics examples
#
# Usage:
#   make              - Build the graphics example
#   make run          - Build and run
#   make clean        - Remove build artifacts

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS += -DAFTER_HOURS_USE_RAYLIB

# Paths - adjust these for your system
RAYLIB_INCLUDE := /opt/homebrew/Cellar/raylib/5.5/include
RAYLIB_LIB := /opt/homebrew/Cellar/raylib/5.5/lib
AFTERHOURS_ROOT := ..

INCLUDES := -I$(RAYLIB_INCLUDE) -isystem $(AFTERHOURS_ROOT)/..

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS := -L$(RAYLIB_LIB) -lraylib -framework OpenGL -framework CoreFoundation
else ifeq ($(UNAME_S),Linux)
    LDFLAGS := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

# Header-only library - no additional source files needed

# Targets
.PHONY: all run clean

all: graphics_example

graphics_example: graphics_example.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@
	@echo "Built: graphics_example"

run: graphics_example
	./graphics_example

clean:
	rm -f graphics_example
	rm -f graphics_example_output.png
	rm -rf graphics_example_captures/
