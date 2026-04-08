CXX            = g++
CXXFLAGS       = -std=c++17 -O2 -Wall -Wextra
TARGET         = snake

.PHONY: all clean run

all: $(TARGET)

$(TARGET): snake.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
