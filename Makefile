# Makefile for SSD Simulator with FTL & GC

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
TARGET = ssd_simulator

# Source files
SOURCES = testshell_new.c ssd_new.c ftl.c nand_flash.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = ssd_new.h ftl.h nand_flash.h

# Build target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)
	@echo "Build complete: ./$(TARGET)"

# Compile individual object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f nand_flash.bin result.txt nand.txt
	@echo "Clean complete"

# Run the simulator
run: $(TARGET)
	./$(TARGET)

# Run with automatic test
test: $(TARGET)
	@echo "Running TestApp1..."
	@echo "testapp1" | ./$(TARGET)
	@echo ""
	@echo "Running TestApp2..."
	@echo "testapp2" | ./$(TARGET)
	@echo ""
	@echo "Running TestApp3 (GC test)..."
	@echo "testapp3" | ./$(TARGET)

# Show statistics
stats: $(TARGET)
	@echo "stats" | ./$(TARGET)

.PHONY: all clean run test stats
