include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../..

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp ../../../src/plugins/files.cpp -o $(OUT)/files_plugin.exe && $(OUT)/files_plugin.exe

