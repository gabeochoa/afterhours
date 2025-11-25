FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -O2 -march=native

all:
	clang++ $(FLAGS) main.cpp -o performance_benchmark.exe && ./performance_benchmark.exe

clean:
	rm -f performance_benchmark.exe performance_benchmark.exe.dSYM

.PHONY: all clean

