include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../.. -I../../../vendor -I../../../../../vendor -DFMT_HEADER_ONLY

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp -o $(OUT)/translation.exe && $(OUT)/translation.exe
