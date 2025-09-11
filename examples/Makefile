CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I../src -DAFTER_HOURS_DEBUG
TARGET = system_map_test
SOURCE = system_map_test.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
