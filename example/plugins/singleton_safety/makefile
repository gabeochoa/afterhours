FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../..

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp ../../../src/plugins/files.cpp -o singleton_safety.exe && ./singleton_safety.exe
