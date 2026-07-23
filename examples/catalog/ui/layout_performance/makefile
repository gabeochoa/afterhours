
# Layout Performance Benchmarks Makefile
#
# Uses -O3 optimization for meaningful benchmark results
# Based on PanGui benchmark methodology

FLAGS = -std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

all: build run

build:
	clang++ $(FLAGS) main.cpp -o layout_performance.exe

run:
	./layout_performance.exe

# Run with verbose output showing individual samples
verbose:
	./layout_performance.exe -d yes

# Run only supported benchmarks (skip stubbed ones)
supported:
	./layout_performance.exe [supported]

# Run layout verification tests (dump computed layouts for validation)
verify:
	./layout_performance.exe [verify] -s

# List available benchmarks
list:
	./layout_performance.exe --list-tests

clean:
	rm -f layout_performance.exe

.PHONY: all build run verbose supported list clean
