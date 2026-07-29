# Makefile for afterhours examples/benchmarks
#
# Usage:
#   make              - Build all examples and benchmarks
#   make run          - Build and run graphics example
#   make test         - Build and run tests from ../tests
#   make clean        - Remove build artifacts

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS_RAYLIB := $(CXXFLAGS) -DAFTER_HOURS_USE_RAYLIB

# Optimized flags for profiling (keeps debug symbols for sampling)
CXXFLAGS_OPT := -std=c++20 -Wall -Wextra -O2 -g -fno-omit-frame-pointer
CXXFLAGS_OPT_RAYLIB := $(CXXFLAGS_OPT) -DAFTER_HOURS_USE_RAYLIB

# Paths
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
# picks up a *sibling* `afterhours` checkout. Generate a private include dir
# containing a symlink named exactly `afterhours` -> this checkout root, and use
# it as the sole afterhours root.
AFTERHOURS_ROOT := $(realpath ..)
INCLUDE_ROOT := $(CURDIR)/.afh-include
$(shell mkdir -p $(INCLUDE_ROOT) && ln -sfn $(AFTERHOURS_ROOT) $(INCLUDE_ROOT)/afterhours)

INCLUDES := -isystem $(INCLUDE_ROOT) -isystem $(AFTERHOURS_ROOT)/vendor -isystem $(BREW_PREFIX)/include
INCLUDES_RAYLIB := -I$(RAYLIB_INCLUDE) $(INCLUDES)

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_RAYLIB := $(RAYLIB_LIB)/libraylib.a -framework OpenGL -framework CoreFoundation -framework Cocoa -framework IOKit -framework CoreGraphics -framework CoreVideo -lobjc
else ifeq ($(UNAME_S),Linux)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

# Header-only library - no additional source files needed

# Targets
.PHONY: all run test clean

all: graphics_example drawing_3d_example headless_validation perf_stress_test calendar_stress_test profile_harness

graphics_example: graphics_example.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: graphics_example"

drawing_3d_example: drawing_3d_example.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: drawing_3d_example"

# Headless validation - compares headless vs windowed rendering output
headless_validation: headless_validation.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: headless_validation"

# imm visual regression - renders imm UI to a hidden window, captures a PNG
imm_visual_check: imm_visual_check.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: imm_visual_check"

# on_draw example - pedagogical demo of with_on_draw_bg / with_on_draw_fg
on_draw_example: on_draw_example.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: on_draw_example"

# Performance stress tests - no raylib dependency
perf_stress_test: perf_stress_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@
	@echo "Built: perf_stress_test"

# Calendar layout stress test - full rendering pipeline
calendar_stress_test: calendar_stress_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: calendar_stress_test"

run: graphics_example
	./graphics_example

run-3d: drawing_3d_example
	./drawing_3d_example

test:
	$(MAKE) -C ../tests test

# Optimized profiling builds
perf_stress_test_opt: perf_stress_test.cpp
	$(CXX) $(CXXFLAGS_OPT) $(INCLUDES) $^ -o $@
	@echo "Built: perf_stress_test_opt (O2)"

calendar_stress_test_opt: calendar_stress_test.cpp
	$(CXX) $(CXXFLAGS_OPT_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: calendar_stress_test_opt (O2)"

profile_harness: profile_harness.cpp
	$(CXX) $(CXXFLAGS_OPT) $(INCLUDES) $^ -o $@
	@echo "Built: profile_harness (O2)"

profile-layout: perf_stress_test_opt
	@echo "Profiling layout benchmark (10s sample)..."
	./perf_stress_test_opt layout &  PERF_PID=$$!; \
	sleep 0.5; \
	sample $$PERF_PID -f profile_layout.txt 2>/dev/null || true; \
	wait $$PERF_PID 2>/dev/null || true
	@echo "Profile saved to profile_layout.txt"

profile-calendar: calendar_stress_test_opt
	@echo "Profiling calendar benchmark (layout-only, 500 frames)..."
	./calendar_stress_test_opt --layout-only --frames=500 --meetings=30 &  PERF_PID=$$!; \
	sleep 0.5; \
	sample $$PERF_PID -f profile_calendar.txt 2>/dev/null || true; \
	wait $$PERF_PID 2>/dev/null || true
	@echo "Profile saved to profile_calendar.txt"

clean:
	rm -f graphics_example drawing_3d_example headless_validation perf_stress_test calendar_stress_test profile_harness
	rm -f perf_stress_test_opt calendar_stress_test_opt
	rm -f imm_visual_check imm_visual_check.png imm_visual_check_batched.png
	rm -f on_draw_example on_draw_example.png
	rm -f graphics_example_output.png
	rm -f profile_layout.txt profile_calendar.txt
	rm -rf graphics_example_captures/
	rm -rf headless_validation_output/
	rm -rf $(INCLUDE_ROOT)
