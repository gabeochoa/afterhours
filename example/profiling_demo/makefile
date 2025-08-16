DEBUG_FLAGS = -std=c++20 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wmost -Wconversion -g -I../../.. -I../../../src -I../../../afterhours/vendor -DDEBUG

RELEASE_FLAGS = -std=c++20 -O2 -DRELEASE -I../../.. -I../../../src -I../../../afterhours/vendor

all: debug release

debug:
	g++ $(DEBUG_FLAGS) main.cpp -o profiling_demo_debug
	@echo "Debug build complete. Running with profiling enabled..."
	./profiling_demo_debug

release:
	g++ $(RELEASE_FLAGS) main.cpp -o profiling_demo_release
	@echo "Release build complete. Running without profiling..."
	-./profiling_demo_release

clean:
	rm -f profiling_demo* *.spall
	rm -rf profiling_demo*.dSYM

.PHONY: all debug release clean
