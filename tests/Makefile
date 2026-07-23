# Makefile for afterhours automated tests
#
# Usage:
#   make        - Build all tests
#   make test   - Build and run all tests
#   make clean  - Remove test binaries

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS_RAYLIB := $(CXXFLAGS) -DAFTER_HOURS_USE_RAYLIB

RAYLIB_INCLUDE := /opt/homebrew/Cellar/raylib/5.5/include
RAYLIB_LIB := /opt/homebrew/Cellar/raylib/5.5/lib
AFTERHOURS_ROOT := ..

INCLUDES := -isystem $(AFTERHOURS_ROOT)/..
INCLUDES_RAYLIB := -I$(RAYLIB_INCLUDE) $(INCLUDES)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_RAYLIB := $(RAYLIB_LIB)/libraylib.a -framework OpenGL -framework CoreFoundation -framework Cocoa -framework IOKit -framework CoreGraphics -framework CoreVideo -lobjc
else ifeq ($(UNAME_S),Linux)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

.PHONY: all test clean

ALL_TESTS := \
	autolayout_test \
	dump_ui_test \
	entity_mapping_test \
	entity_pool_test \
	entity_query_test \
	input_injector_mouse_delta_test \
	progress_bar_test \
	render_order_test \
	slider_test \
	stepper_test \
	tab_container_test \
	text_wrap_test

RAYLIB_TESTS := \
	dump_ui_test \
	progress_bar_test \
	render_order_test \
	slider_test \
	stepper_test \
	tab_container_test \
	text_wrap_test

all: $(ALL_TESTS)

autolayout_test: autolayout_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

entity_mapping_test: entity_mapping_test.cpp ../examples/entity_mapping_test_helper.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

entity_pool_test: entity_pool_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

entity_query_test: entity_query_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

input_injector_mouse_delta_test: input_injector_mouse_delta_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

dump_ui_test: dump_ui_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

progress_bar_test: progress_bar_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

render_order_test: render_order_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

slider_test: slider_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

stepper_test: stepper_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

tab_container_test: tab_container_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

text_wrap_test: text_wrap_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(INCLUDES_RAYLIB) $^ $(LDFLAGS_RAYLIB) -o $@

test: all
	@for t in $(ALL_TESTS); do \
		echo "Running $$t"; \
		./$$t; \
	done

clean:
	rm -f $(ALL_TESTS) input_system_mouse_delta_backend_smoke.o smoke_raylib.o smoke_sokol.o
