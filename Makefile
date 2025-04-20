# Makefile for Satellite Ground Station Program

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -pthread

# Target executable
TARGET = satellite_ground_station

# Source files
SRCS = satellite_ground_station.c

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