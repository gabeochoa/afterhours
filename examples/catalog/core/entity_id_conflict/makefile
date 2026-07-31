include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../../vendor/ -I../../../../../vendor \
		-DFMT_HEADER_ONLY

all:
	clang++ $(FLAGS) main.cpp -o $(OUT)/entity_id_conflict.exe && $(OUT)/entity_id_conflict.exe 