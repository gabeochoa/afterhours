CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I../../src -DAFTER_HOURS_DEBUG

# Target executable
TARGET = translation_unit_test

# Source files
SOURCES = main.cpp other_file.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run the test
run: $(TARGET)
	./$(TARGET)

# Show what this test demonstrates
info:
	@echo "This test demonstrates the static initialization order fiasco"
	@echo "across multiple translation units in C++."
	@echo ""
	@echo "The same component types (Transform, Velocity) are defined"
	@echo "in both main.cpp and other_file.cpp, causing them to get"
	@echo "different component type IDs in each translation unit."
	@echo ""
	@echo "This leads to systems not finding entities because their"
	@echo "component bitsets don't match what's stored in the system map."
	@echo ""
	@echo "Run 'make run' to see the issue in action."

.PHONY: all clean run info
