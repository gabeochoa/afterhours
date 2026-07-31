include ../../common.mk
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2
TARGET = $(OUT)/bitset_utils_example.exe
SOURCE = main.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	$(TARGET)

.PHONY: clean run
