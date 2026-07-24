# Animation plugin update() micro-benchmark.
# -O3 -DNDEBUG for meaningful numbers. Sweeps N tracks x active-fraction.
#
# NOTE: on the Santa-locked dev machine freshly-compiled binaries are SIGKILLed
# (exit 137), so `run` won't produce numbers there. Build is the floor.
FLAGS = -std=c++23 -O3 -DNDEBUG -Wall -Wextra -I ../../../../vendor

all: build run

build:
	clang++ $(FLAGS) animation_bench.cpp -o animation_bench.exe

run:
	./animation_bench.exe

clean:
	rm -f animation_bench.exe

.PHONY: all build run clean
