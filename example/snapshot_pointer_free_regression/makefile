
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g \
		--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13

all:
	clang++ $(FLAGS) main.cpp -o snapshot_pointer_free_regression.exe && ./snapshot_pointer_free_regression.exe

