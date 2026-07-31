include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/library_usage.exe && $(OUT)/library_usage.exe
