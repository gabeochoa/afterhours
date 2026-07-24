
# Timer Plugin Performance Benchmarks Makefile
#
# Uses -O3 -DNDEBUG for meaningful benchmark results.
# NOTE: on a Santa-Lockdown machine the produced binary is SIGKILLed (exit 137)
# and cannot run — build it and run on an unlocked machine.

FLAGS = -std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow

all: build run

build:
	clang++ $(FLAGS) main.cpp -o timer_benchmarks.exe

run:
	./timer_benchmarks.exe

verbose:
	./timer_benchmarks.exe -d yes

list:
	./timer_benchmarks.exe --list-tests

clean:
	rm -f timer_benchmarks.exe

.PHONY: all build run verbose list clean
