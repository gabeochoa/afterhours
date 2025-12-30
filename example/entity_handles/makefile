FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g

all:
	clang++ $(FLAGS) main.cpp -o entity_handles.exe && ./entity_handles.exe
	clang++ $(FLAGS) -DAFTER_HOURS_ASSIGN_HANDLES_ON_CREATE main.cpp -o entity_handles_handles_on_create.exe && ./entity_handles_handles_on_create.exe

