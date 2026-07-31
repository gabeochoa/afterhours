include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/tests.exe && $(OUT)/tests.exe
