
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g

all:
	clang++ $(FLAGS) main.cpp entity_handle_tests.cpp -o tests.exe && ./tests.exe
	clang++ $(FLAGS) -DAFTER_HOURS_ASSIGN_HANDLES_ON_CREATE main.cpp entity_handle_tests.cpp -o tests_handles_on_create.exe && ./tests_handles_on_create.exe
