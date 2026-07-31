include ../../common.mk
ROOT = ../../../..
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		 -Wconversion -g -isystem $(ROOT)/vendor \
		 -isystem $(shell brew --prefix)/include -DFMT_HEADER_ONLY

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/split_layout.exe && $(OUT)/split_layout.exe
