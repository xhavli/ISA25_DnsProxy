# Project: ISA25 Filter Resolver
# Author: Adam Havlik (xhavli59)
# Date: 17.11.2025

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

# Run Python tests using pytest
.PHONY: test
test: $(TARGET)
	@pytest -q --disable-warnings --maxfail=1 || (rm -f $(TARGET) && exit 1)
	@$(MAKE) clean --no-print-directory

# Clean up the project and Python cache
clean:
	@rm -f $(TARGET)
	@$(MAKE) clean-pycache --no-print-directory

# Remove Python cache
clean-pycache:
	@find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null
	@find . -type f -name "*.pyc" -delete 2>/dev/null