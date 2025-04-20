# Makefile for Satellite Ground Station Program

# Compiler
CC = gcc

# Compiler flags (match exact compilation command from assignment)
CFLAGS = -Wall -pthread

# Target executable
TARGET = hw3

# Source files
SRCS = main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean