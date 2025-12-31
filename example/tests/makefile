
# NOTE: our clang package can auto-detect a GCC "14" layout that isn't actually
# installed in this environment, which breaks standard library headers.
# Pin clang to the GCC 13 toolchain we do have.
CXX = clang++
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g \
		--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13

all:
	$(CXX) $(FLAGS) main.cpp -o tests.exe && ./tests.exe
