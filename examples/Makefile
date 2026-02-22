# Makefile for afterhours examples and tests
#
# Usage:
#   make              - Build all examples and tests
#   make run          - Build and run graphics example
#   make test         - Build and run autolayout tests
#   make clean        - Remove build artifacts

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS_RAYLIB := $(CXXFLAGS) -DAFTER_HOURS_USE_RAYLIB

# Paths - adjust these for your system
RAYLIB_INCLUDE := /opt/homebrew/Cellar/raylib/5.5/include
RAYLIB_LIB := /opt/homebrew/Cellar/raylib/5.5/lib
AFTERHOURS_ROOT := ..

INCLUDES := -isystem $(AFTERHOURS_ROOT)/..
INCLUDES_RAYLIB := -I$(RAYLIB_INCLUDE) $(INCLUDES)

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -framework OpenGL -framework CoreFoundation
else ifeq ($(UNAME_S),Linux)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

# Header-only library - no additional source files needed

# Targets
.PHONY: all run test clean

all: graphics_example autolayout_test drawing_3d_example

graphics_example: graphics_example.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: graphics_example"

drawing_3d_example: drawing_3d_example.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: drawing_3d_example"

# Autolayout tests - pure C++, no raylib dependency
autolayout_test: autolayout_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@
	@echo "Built: autolayout_test"

run: graphics_example
	./graphics_example

run-3d: drawing_3d_example
	./drawing_3d_example

test: autolayout_test drawing_3d_example
	./autolayout_test
	./drawing_3d_example

clean:
	rm -f graphics_example autolayout_test drawing_3d_example
	rm -f graphics_example_output.png
	rm -rf graphics_example_captures/
