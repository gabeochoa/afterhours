include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../vendor/ -I../../../../vendor

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/tag_filters.exe && $(OUT)/tag_filters.exe

