include ../../common.mk
all: general_test overflow_test force_test

general_test:
	@echo "=== Running General Component Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g main.cpp -o $(OUT)/component_overflow_test.exe && $(OUT)/component_overflow_test.exe
	@echo ""

overflow_test:
	@echo "=== Running Specific Overflow Trigger Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g overflow_trigger.cpp -o $(OUT)/overflow_trigger.exe && $(OUT)/overflow_trigger.exe
	@echo ""

force_test:
	@echo "=== Running Forced Overflow Test ==="
	clang++ -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wmost -Wconversion -g force_overflow_test.cpp -o $(OUT)/force_overflow_test.exe && $(OUT)/force_overflow_test.exe

clean:
	rm -f *.exe

.PHONY: all general_test overflow_test force_test clean