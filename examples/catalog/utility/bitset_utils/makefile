CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2
TARGET = bitset_utils_example
SOURCE = main.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
