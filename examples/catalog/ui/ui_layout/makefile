
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

ui_layout.exe: main.cpp
	clang++ $(FLAGS) main.cpp -o ui_layout.exe

all: ui_layout.exe
	./ui_layout.exe
