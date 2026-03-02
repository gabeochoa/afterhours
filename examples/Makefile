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

# Optimized flags for profiling (keeps debug symbols for sampling)
CXXFLAGS_OPT := -std=c++20 -Wall -Wextra -O2 -g -fno-omit-frame-pointer
CXXFLAGS_OPT_RAYLIB := $(CXXFLAGS_OPT) -DAFTER_HOURS_USE_RAYLIB

# Paths - adjust these for your system
RAYLIB_INCLUDE := /opt/homebrew/Cellar/raylib/5.5/include
RAYLIB_LIB := /opt/homebrew/Cellar/raylib/5.5/lib
AFTERHOURS_ROOT := ..

INCLUDES := -isystem $(AFTERHOURS_ROOT)/..
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

all: graphics_example autolayout_test drawing_3d_example entity_mapping_test entity_pool_test headless_validation perf_stress_test calendar_stress_test

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

# Headless validation - compares headless vs windowed rendering output
headless_validation: headless_validation.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@
	@echo "Built: headless_validation"

# Entity mapping tests - multi-TU test for ENTITY_ID_GEN and UIEntityMappingCache
entity_mapping_test: entity_mapping_test.cpp entity_mapping_test_helper.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@
	@echo "Built: entity_mapping_test"

entity_pool_test: entity_pool_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@
	@echo "Built: entity_pool_test"

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

test: autolayout_test drawing_3d_example entity_mapping_test entity_pool_test
	./autolayout_test
	./drawing_3d_example
	./entity_mapping_test
	./entity_pool_test

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
	rm -f graphics_example autolayout_test drawing_3d_example entity_mapping_test entity_pool_test headless_validation perf_stress_test calendar_stress_test
	rm -f perf_stress_test_opt calendar_stress_test_opt
	rm -f graphics_example_output.png
	rm -f profile_layout.txt profile_calendar.txt
	rm -rf graphics_example_captures/
	rm -rf headless_validation_output/
