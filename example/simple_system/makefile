
FLAGS = -std=c++2a -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g $(RAYLIB_FLAGS) -DTRACY_ENABLE $(TIMEFLAG)

all:
	clang++ $(FLAGS) main.cpp -o simple_system.exe && ./simple_system.exe
