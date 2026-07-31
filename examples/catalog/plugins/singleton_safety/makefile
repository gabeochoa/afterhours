include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../..

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp ../../../src/plugins/files.cpp -o $(OUT)/singleton_safety.exe && $(OUT)/singleton_safety.exe
