FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g

all:
	clang++ $(FLAGS) main.cpp -o text_cache_usage.exe && ./text_cache_usage.exe

clean:
	rm -f text_cache_usage.exe
	rm -rf text_cache_usage.exe.dSYM

.PHONY: all clean

