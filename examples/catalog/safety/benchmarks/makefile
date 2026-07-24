
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
	rm -f ecs_benchmarks.exe camera_bench.exe

# Camera plugin benchmark — isolates the get_singleton_cmp<HasCamera> per-frame
# lookup cost (docs/speed/camera.md item #1). Backend-independent (no raylib).
camera_bench:
	clang++ $(FLAGS) -I../../../../vendor/ camera_bench.cpp -o camera_bench.exe
	./camera_bench.exe

camera_bench_build:
	clang++ $(FLAGS) -I../../../../vendor/ camera_bench.cpp -o camera_bench.exe

.PHONY: all build run verbose list clean camera_bench camera_bench_build
