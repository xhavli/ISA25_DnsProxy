# Compiler and flags
COMPILER = g++
COMPILERFLAGS = -Wall -Wextra -std=c++17 -O1 #-Werror

# Output file name and source files
TARGET = dns
# Source files - all .cpp files in root and subdirectories
SOURCES := $(wildcard *.cpp */*.cpp)
# Include directories (add all folders with headers)
INCLUDES := -I. -Idns_flags -Ifilter_helper -Iprint_helper -Istructures

# Default target
all: $(TARGET)

# Rule for linking the executable - link everything directly — no *.o files
$(TARGET):
	$(COMPILER) $(COMPILERFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES)

# Clean up the project
clean:
	rm -f $(TARGET)