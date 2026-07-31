include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../../vendor/ -I../../../../../vendor -DFMT_HEADER_ONLY

$(OUT)/ui_layout.exe: main.cpp
	clang++ $(FLAGS) main.cpp -o $(OUT)/ui_layout.exe

all: $(OUT)/ui_layout.exe
	$(OUT)/ui_layout.exe
