include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		 -Wconversion -g -I../../../vendor/ -I../../../../../vendor

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/render_batching.exe && $(OUT)/render_batching.exe
