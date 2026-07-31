include ../../common.mk
# ECS Performance Benchmarks Makefile
#
# Uses -O3 optimization for meaningful benchmark results

FLAGS = -std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

all: build run

build:
	clang++ $(FLAGS) main.cpp -o $(OUT)/ecs_benchmarks.exe

run:
	$(OUT)/ecs_benchmarks.exe

# Run with verbose output showing individual samples
verbose:
	$(OUT)/ecs_benchmarks.exe -d yes

# List available benchmarks
list:
	$(OUT)/ecs_benchmarks.exe --list-tests

clean:
	rm -f $(OUT)/ecs_benchmarks.exe

.PHONY: all build run verbose list clean
