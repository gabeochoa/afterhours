include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/text_cache_usage.exe && $(OUT)/text_cache_usage.exe

clean:
	rm -f $(OUT)/text_cache_usage.exe
	rm -rf $(OUT)/text_cache_usage.exe.dSYM

.PHONY: all clean

