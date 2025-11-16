# Compiler and flags
COMPILER = g++
FLAGS = -Wall -Wextra -std=c++17 -O2

# Output file name and source files
TARGET = dns
SOURCES = main.cpp filter_helper.cpp print_helper.cpp

# Default target
all: $(TARGET)

# Rule for linking the executable
$(TARGET): $(SOURCES)
	$(COMPILER) $(FLAGS) -o $@ $^

# Clean up the project
clean:
	rm -f $(TARGET)