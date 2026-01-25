all: general_test overflow_test force_test

general_test:
	@echo "=== Running General Component Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g main.cpp -o component_overflow_test.exe && ./component_overflow_test.exe
	@echo ""

overflow_test:
	@echo "=== Running Specific Overflow Trigger Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g overflow_trigger.cpp -o overflow_trigger.exe && ./overflow_trigger.exe
	@echo ""

force_test:
	@echo "=== Running Forced Overflow Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g force_overflow_test.cpp -o force_overflow_test.exe && ./force_overflow_test.exe

clean:
	rm -f *.exe

.PHONY: all general_test overflow_test force_test clean