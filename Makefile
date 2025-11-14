# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2

# Output file name and source files
TARGET = dns
SRC = main.cpp proxy_config.cpp

# Default target
all: $(TARGET)

# Rule for linking the executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Run the program (does not require sudo)
run: $(TARGET)
	./$(TARGET)

# Clean up the project
clean:
	rm -f $(TARGET)