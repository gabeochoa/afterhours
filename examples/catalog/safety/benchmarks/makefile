
# ECS Performance Benchmarks Makefile
#
# Uses -O3 optimization for meaningful benchmark results

FLAGS = -std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

all: build run

build:
	clang++ $(FLAGS) main.cpp -o ecs_benchmarks.exe

run:
	./ecs_benchmarks.exe

# Run with verbose output showing individual samples
verbose:
	./ecs_benchmarks.exe -d yes

# List available benchmarks
list:
	./ecs_benchmarks.exe --list-tests

clean:
	rm -f ecs_benchmarks.exe

.PHONY: all build run verbose list clean
